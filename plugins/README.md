# NestDAQ FairMQ Plugins

[English](README.md) | [日本語](README.ja.md)

[Top: NestDAQ](../README.md) | [Previous: Scripts](../scripts/README.md) | [Next: Web controller](../controller/README.md)

NestDAQ installs three FairMQ plugins as shared libraries:

| Plugin name        | Library                               | Purpose |
|--------------------|----------------------------------------|---------|
| `daq_service`      | `libFairMQPlugin_daq_service.so`       | Registers the FairMQ device in Redis, writes health/state and topology/channel data, and handles data acquisition (DAQ) commands. |
| `metrics`          | `libFairMQPlugin_metrics.so`           | Writes process metrics and FairMQ channel throughput metrics to Redis and RedisTimeSeries. |
| `parameter_config` | `libFairMQPlugin_parameter_config.so`  | Reads parameters from Redis and mirrors them into FairMQ program properties. |

Each loaded plugin requires access to a Redis server for its intended operation.
RedisTimeSeries is required only when the `metrics` plugin is loaded because that plugin creates and updates time-series keys.
The `daq_service` and `parameter_config` plugins use core Redis commands and do not require RedisTimeSeries.

FairMQ and the executable that uses it define the exact option for loading plugins.
Use the plugin names above when enabling these libraries.

In the key patterns below, `{sep}` represents the configured separator.
The default separator is `:`.
The other placeholders are `{service}`, `{id}`, `{channel}`, and `{subindex}`.

## 1. Time To Live (TTL) Behavior

TTL handling is different for each plugin:

- `daq_service` manages Redis key expiration.
  It refreshes registry keys while the device is alive and uses expiration as a fallback cleanup mechanism when a device terminates unexpectedly.
- `metrics` does not generally set Redis key TTLs for metric hashes.
  Instead, `--metrics-max-ttl` specifies how long fields may remain after an instance's last metrics update before the plugin removes them from the metric hashes.
  `--retention` controls RedisTimeSeries retention separately.
- `parameter_config` does not set TTLs on parameter keys.
  The producer or operator that writes those Redis keys controls their lifetime.

## 2. daq_service

`daq_service` is a Redis service-registry plugin.
It registers a device instance, refreshes TTLs, writes FairMQ state, health, topology, and channel data, and subscribes to DAQ commands.

<a id="21-runtime-options"></a>
### 2.1. Command-Line Options

All command-line options in this document are optional.
When an option is omitted, the plugin uses the default shown in its table.

| Option                           | Default                    | Description |
|----------------------------------|----------------------------|-------------|
| `--service-name`                 | executable basename when empty | Service name of this NestDAQ device process, used in Redis key paths and the health `serviceName` field. |
| `--uuid`                         | generated                  | UUID of this NestDAQ device process. This value supplies the default telemetry `service.instance.id` unless `--otel-service-instance-id` is set. When `--uuid` is omitted, the standard FairMQ device wrapper copies its generated telemetry UUID to this property; if the property is absent, the plugin generates a UUID. |
| `--host-ip`                      | detected/configured value  | Address of this NestDAQ device process, stored in the health `hostIp` field. A resolvable hostname is accepted. If omitted, the plugin uses the configured network interface or the default-route interface. |
| `--hostname`                     | detected/configured value  | Host name stored in the health `hostName` field. If omitted, the plugin uses the operating system hostname. |
| `--registry-uri`                 | `tcp://127.0.0.1:6379/0`   | Redis uniform resource identifier (URI) for the DAQ service registry. |
| `--separator`                    | `:`                        | Separator used when composing Redis keys. |
| `--max-ttl`                      | `5`                        | TTL in seconds for transient registry keys. |
| `--ttl-update-interval`          | `3`                        | TTL refresh interval in seconds. |
| `--startup-state`                | `idle`                     | FairMQ state to which the plugin automatically advances the device from `Idle` during startup: `idle`, `initializing-device`, `initialized`, `bound`, `device-ready`, `ready`, or `running`. |
| `--enable-uds`                   | `true`                     | Adds Unix domain socket (UDS) addresses only to ZeroMQ bind channels whose peers all have the same `hostIp` as this process. `true` and `1` enable it. |
| `--connect-config`               | none                       | JavaScript Object Notation (JSON) string describing temporary message queue (MQ) channel connection parameters. Section 2.5.1 describes its peer syntax. |
| `--max-retry-to-resolve-address` | `10`                       | Maximum retry count for resolving connect addresses. |

### 2.2. DAQ Service Identity Defaults

`daq_service` uses `--service-name` as this NestDAQ device process's service name in Redis key paths, the health `serviceName` field, and controller displays.
When `--service-name` is not set or is empty, the plugin uses the final path component of the executable name.

When set, the FairMQ `--id` option supplies the NestDAQ service instance id.
When `--id` is not set or is empty, `daq_service` allocates a numeric index in `daq_service{sep}service-instance-index{sep}{service}` and sets the instance id to `{service-name}-{index}`, such as `Sampler-0`.
The `--uuid` value is separate from the instance id and identifies the process for presence, health, and index reuse.
It also supplies the default telemetry `service.instance.id` unless `--otel-service-instance-id` is set explicitly.
When `--uuid` is omitted, the standard FairMQ device wrapper copies its generated telemetry UUID to the `uuid` property; if no `uuid` property exists, the plugin generates one.

### 2.3. Redis Keys Written or Read

Health data is a Redis hash containing device identity, host details, FairMQ state, and lifecycle timestamps.
`TopologyConfig` uses its `hostIp` field for connection resolution, and monitoring clients can read the other fields to report device status.

The `Writer / reader` column names the in-tree components that directly perform each Redis write or read, including scans and Pub/Sub operations.
`daq_service` means the plugin loaded in a NestDAQ device process, `daq-webctl` means the web controller, and `operator` means an external Redis client.

`createdTime`, `updated_time`, `updatedTime`, `start_time`, and `stop_time` are local-time strings in `YYYY-MM-DDTHH:MM:SS` format, with second precision and no time-zone offset.
`uptime` is the number of elapsed milliseconds since the `daq_service` plugin was created.
`start_time_ns` and `stop_time_ns` are elapsed nanoseconds from the same starting point, not Unix epoch timestamps.

| Key pattern | Redis type | Fields / value | Writer / reader | Purpose |
|-------------|------------|----------------|-----------------|---------|
| `daq_service{sep}{service}{sep}{id}{sep}presence` | string | UUID string, refreshed with TTL | Written/read by `daq_service` | Presence marker for one device instance. |
| `daq_service{sep}{service}{sep}{id}{sep}health` | hash | `instanceID`, `uuid`, `hostName`, `hostIp`, `serviceName`, `fair:mq:state`, `createdTime`, `updated_time`, `uptime`; also `start_time`, `start_time_ns`, `stop_time`, `stop_time_ns` when run timing is recorded | Written/read by `daq_service` | Health and lifecycle metadata for one device instance. |
| `daq_service{sep}{service}{sep}{id}{sep}fair-mq-state` | string | FairMQ state name | Written/read by `daq_service`; read by `daq-webctl` | Current FairMQ state with TTL. |
| `daq_service{sep}{service}{sep}{id}{sep}updatedTime` | string | Last update timestamp | Written by `daq_service`; read by `daq-webctl` | Lightweight last-update key with TTL. |
| `daq_service{sep}{service}{sep}{id}{sep}option` | hash | Selected FairMQ program options such as `severity`, `file-severity`, `verbosity`, `color`, `log-to-file`, `id`, `io-threads`, `transport`, `network-interface`, `init-timeout`, shared-memory options, `rate`, and `session` | Written by `daq_service` | Current option values for monitoring and debugging. |
| `daq_service{sep}service-instance-index{sep}{service}` | hash | Field: numeric instance index; value: UUID | Read/write by `daq_service`; deleted by `daq-webctl` after presence expiry | Allocates and reuses `{service}-{index}` instance IDs when `--id` is not given. |
| `run_info{sep}run_number` | string integer | Current or next run number | Read by `daq_service`; read/write by `daq-webctl` | Source for run number metadata. The web controller may increment it and copy it to `latest_run_number` before `RUN`. |
| `run_info{sep}latest_run_number` | string integer | Last run number copied when `RUN` was requested | Read/write by `daq-webctl` | Run number snapshot used for run metadata and display. |
| `run_info{sep}wait-device-ready` | string boolean | `1` or `true`: enabled; missing or any other value: disabled | Read/write by `daq-webctl` | When enabled, `daq-webctl` waits after sending `CONNECT`. Before `INIT TASK` or `RUN`, it first sends `CONNECT` and waits until all discovered target devices report the same accepted state: `DeviceReady`, `Ready`, or `Running`. |
| `run_info{sep}wait-ready` | string boolean | `1` or `true`: enabled; missing or any other value: disabled | Read/write by `daq-webctl` | When enabled, `daq-webctl` waits after sending `INIT TASK`. Before `RUN`, it first sends `INIT TASK` and waits until all discovered target devices report `Ready` or all report `Running`. |
| `daqctl` | pub/sub channel | JSON DAQ command messages | Published by `daq-webctl` or an operator; subscribed by `daq_service` | Receives controller commands. |

### 2.4. DAQ Command Publish/Subscribe (Pub/Sub)

`daq_service` subscribes to `daqctl` and translates matching command messages into FairMQ state transitions for the local service instance.
Controllers and other operators publish command messages to this channel.

Redis Pub/Sub delivers each `daqctl` message to every user device process subscribed to the channel.
Redis does not filter messages by service or instance.
Each subscriber's `daq_service` plugin reads the command, checks whether it selects the local `service-name` and long instance id such as `Sampler-0`, and ignores the command when the local process is not a target.

Messages published to `daqctl` have this shape:

```json
{
  "command": "change_state",
  "value": "RUN",
  "services": ["Sampler", "Sink"],
  "instances": ["Sampler-0", "Sink-0"]
}
```

The `services` array selects service names, and the `instances` array selects instance ids.
Both arrays must be present and non-empty, and each can contain multiple entries.
The plugin stores the entries as sets, so their order and duplicates do not affect target matching.
A device processes the message only when the target selection matches its local service instance.
The `value` field accepts one of the following FairMQ or NestDAQ command strings handled by the plugin:

```text
BIND, COMPLETE INIT, CONNECT, END, INIT DEVICE, INIT TASK, RESET DEVICE,
RESET TASK, RUN, STOP, exit, quit, reset, start
```

Target selection supports the special lowercase string `"all"`:

- `services: ["all"]` targets every device, regardless of `instances`.
- `services: ["Sampler"]` with `instances: ["all"]` targets every instance of
  the `Sampler` service.
- `services: ["Sampler"]` with `instances: ["Sampler-0"]` targets only the
  `Sampler-0` instance.
- Other devices ignore the message.

The implementation compares the literal string `"all"` without case conversion.
Use lowercase `"all"`, not `"ALL"` or `"All"`.

Examples:

```json
{
  "command": "change_state",
  "value": "STOP",
  "services": ["all"],
  "instances": ["all"]
}
```

```json
{
  "command": "change_state",
  "value": "RUN",
  "services": ["Sampler"],
  "instances": ["all"]
}
```

```json
{
  "command": "change_state",
  "value": "RUN",
  "services": ["Sampler"],
  "instances": ["Sampler-0"]
}
```

Target multiple services and every instance under those services:

```json
{
  "command": "change_state",
  "value": "CONNECT",
  "services": ["Sampler", "Sink"],
  "instances": ["all"]
}
```

Target selected instances across services:

```json
{
  "command": "change_state",
  "value": "RUN",
  "services": ["Sampler", "Sink"],
  "instances": ["Sampler-0", "Sampler-1", "Sink-0"]
}
```

The last message is still delivered to every `daqctl` subscriber.
For example, `Sampler-2` and `Sink-1` receive the message but ignore it because their long instance ids are not listed in `instances`.

When the web controller requests `RUN`, it copies `run_info{sep}run_number` to `run_info{sep}latest_run_number`.
According to `run_info{sep}wait-device-ready` and `run_info{sep}wait-ready`, the controller then publishes any prerequisite `CONNECT` and `INIT TASK` commands, publishes `RUN`, and runs its configured pre/post hooks.
When the controller requests `STOP`, it publishes `STOP` and runs its configured pre/post hooks.

The web controller's prerequisite wait logic uses the same target selection.
`services: ["all"]` waits on all known service and instance state keys, whereas `instances: ["all"]` waits on all instances under the selected services.

`daq_service` writes the current state to `daq_service{sep}{service}{sep}{id}{sep}fair-mq-state` and refreshes the related presence, health, and timestamp keys.
Controllers such as `daq-webctl` can poll or scan these keys to build state summaries.

### 2.5. Topology and Channel Keys

`daq_service` also writes channel metadata through `TopologyConfig`.

| Key pattern | Redis type | Fields / value | Writer / reader | Purpose |
|-------------|------------|----------------|-----------------|---------|
| `daq_service{sep}{service}{sep}{id}{sep}channel{sep}{channel}` | hash | `name`, `type`, `method`, `address`, `transport`, buffer sizes, kernel sizes, `linger`, `rateLogging`, port range, `autoBind`, `numSockets`, `autoSubChannel`, `bound`, `waitForPeerConnection` | Written/read | Stored channel endpoint metadata. |
| `daq_service{sep}{service}{sep}{id}{sep}channel{sep}{channel}{sep}peer` | list | Peer channel key strings | Written/read | Peer list for the channel. |
| `daq_service{sep}{service}{sep}{id}{sep}socket{sep}chans.{channel}.{subindex}` | hash | Local subchannel/socket parameters plus `numSockets` and `autoSubChannel` | Written/read | Per-subchannel connection metadata. |
| `daq_service{sep}topology{sep}endpoint...` | string/hash keys | Topology endpoint configuration | Read/scanned | External topology configuration used to resolve endpoints. |
| `daq_service{sep}topology{sep}link...` | string/hash keys | Topology link configuration | Read/scanned | External topology configuration used to resolve links between services/channels. |

#### 2.5.1. `autoSubChannel`

FairMQ stores each named channel as a `std::vector<fair::mq::Channel>`.
Each `fair::mq::Channel` wraps one FairMQ Socket, and the vector index identifies
a subchannel.
Device code selects a local subchannel with the index argument of `Send()` or
`Receive()`; omitting that argument selects index `0`.

In the JSON passed to the NestDAQ `daq_service` plugin's `--connect-config`
option, a numeric suffix such as `[0]` selects a peer subchannel.
For example, `Sampler:Sampler-0:out[0]` selects subchannel `0` of the peer's
`out` channel.
This is JSON notation parsed by `TopologyConfig`, not C++ syntax or syntax used
by a topology shell script's `link` command.
The `{subindex}` text in the Redis key table above is a placeholder, whereas
`[0]` is a suffix written in a `peer` string:

```json
{"in":{"type":"pull","peer":"Sampler:Sampler-0:out[0]"}}
```

The top-level key names the local channel, and `peer` accepts either one string
or an array of strings.

`autoSubChannel` controls how `TopologyConfig` expands a peer for which this
subchannel suffix is omitted. Its default is `false`.

- `autoSubChannel=false` resolves an unindexed peer to subchannel `0` only.
  This setting is suitable for 1:1 or other fixed connections.
- `autoSubChannel=true` scans the peer-channel subchannel records already stored in Redis and connects to all matching subchannels.
  This setting is suitable for n:m topologies in which the process discovers the number of peers or sockets while running.
- When the peer string includes a suffix such as `[0]`, only that subchannel is resolved, regardless of `autoSubChannel`.

The following diagram shows how each side's `autoSubChannel` setting changes the number of address-bearing channel sockets when a topology connects two services with different process counts.
The diagram illustrates socket and subchannel counts, not fixed port assignments or message direction.
Invisible layout links keep `Sampler` on the left and `Sink` on the right; they are not data paths.

```mermaid
flowchart LR
    Topology["Topology link: <br/> Sampler:out <-> Sink:in<br/>Sampler has 3 processes; <br/> Sink has 2 processes"]

    subgraph CaseFF["Sampler autoSubChannel=false; Sink autoSubChannel=false"]
        direction LR
        subgraph SFF["Sampler"]
            SFF0["Sampler-1<br/>out[0] address:port"]
            SFF1["Sampler-2<br/>out[0] address:port"]
            SFF2["Sampler-0<br/>out[0] address:port"]
        end
        subgraph KFF["Sink"]
            KFF0["Sink-0<br/>in[0] address:port"]
            KFF1["Sink-1<br/>in[0] address:port"]
        end
        SFF2 ~~~ KFF0
    end

    subgraph CaseTF["Sampler autoSubChannel=true; Sink autoSubChannel=false"]
        direction LR
        subgraph STF["Sampler"]
            STF0["Sampler-1<br/>out[0] address:port<br/>out[1] address:port"]
            STF1["Sampler-2<br/>out[0] address:port<br/>out[1] address:port"]
            STF2["Sampler-0<br/>out[0] address:port<br/>out[1] address:port"]
        end
        subgraph KTF["Sink"]
            KTF0["Sink-0<br/>in[0] address:port"]
            KTF1["Sink-1<br/>in[0] address:port"]
        end
        STF2 ~~~ KTF0
    end

    subgraph CaseFT["Sampler autoSubChannel=false; Sink autoSubChannel=true"]
        direction LR
        subgraph SFT["Sampler"]
            SFT0["Sampler-1<br/>out[0] address:port"]
            SFT1["Sampler-2<br/>out[0] address:port"]
            SFT2["Sampler-0<br/>out[0] address:port"]
        end
        subgraph KFT["Sink"]
            KFT0["Sink-0<br/>in[0] address:port<br/>in[1] address:port<br/>in[2] address:port"]
            KFT1["Sink-1<br/>in[0] address:port<br/>in[1] address:port<br/>in[2] address:port"]
        end
        SFT2 ~~~ KFT0
    end

    subgraph CaseTT["Sampler autoSubChannel=true; Sink autoSubChannel=true"]
        direction LR
        subgraph STT["Sampler"]
            STT0["Sampler-1<br/>out[0] address:port<br/>out[1] address:port"]
            STT1["Sampler-2<br/>out[0] address:port<br/>out[1] address:port"]
            STT2["Sampler-0<br/>out[0] address:port<br/>out[1] address:port"]
        end
        subgraph KTT["Sink"]
            KTT0["Sink-0<br/>in[0] address:port<br/>in[1] address:port<br/>in[2] address:port"]
            KTT1["Sink-1<br/>in[0] address:port<br/>in[1] address:port<br/>in[2] address:port"]
        end
        STT2 ~~~ KTT0
    end

    Topology --- CaseFF
    Topology --- CaseTF
    Topology --- CaseFT
    Topology --- CaseTT
```

The plugin normally calculates `numSockets` from the topology.
For channels with `autoSubChannel=true`, `numSockets` grows with the discovered peer instances and subchannels so that each FairMQ sub-socket can receive a distinct `address:port` and subchannel index.

#### 2.5.2. Bind/Connect Sequence

`TopologyConfig` synchronizes bind and connect endpoints through Redis during FairMQ state transitions.

```mermaid
sequenceDiagram
    participant Device
    participant TopologyConfig
    participant Redis
    participant PeerDevices as Peer devices
    participant FairMQProperties as FairMQ properties

    Device->>TopologyConfig: InitializingDevice
    TopologyConfig->>Redis: read topology endpoints and links
    TopologyConfig->>TopologyConfig: classify bind/connect channels
    TopologyConfig->>Redis: scan peer presence keys
    TopologyConfig->>TopologyConfig: update numSockets when autoSubChannel=true
    TopologyConfig->>Redis: write channel metadata and peer lists
    TopologyConfig->>FairMQProperties: set initial chans.* properties

    Device->>TopologyConfig: Bound
    alt bind channels exist
        TopologyConfig->>Redis: write local socket address records
        TopologyConfig->>Redis: mark bind channels bound=1
    end
    alt connect channels exist
        TopologyConfig->>Redis: wait for peer bind channels bound=1
        alt explicit connect-config is set
            TopologyConfig->>Redis: read peer health and socket records
            TopologyConfig->>FairMQProperties: configConnect() sets connect addresses
        else topology links are used
            TopologyConfig->>Redis: read peer lists and socket records
            TopologyConfig->>FairMQProperties: resolveConnectAddress() sets connect addresses
        end
        TopologyConfig->>Redis: write resolved connect channel addresses
    end
    alt waitForPeerConnection=true on bind channels
        TopologyConfig->>Redis: read peer FairMQ states
        Redis-->>TopologyConfig: peer states are connection-ready
    end
```

Bind channels write their local addresses to Redis first.
Connect channels wait for the peer bind channel to become `bound=1`, resolve the peer socket addresses from Redis, and write the resulting FairMQ `chans.*` properties.
A bind channel with `waitForPeerConnection=false` skips the final wait for the peer to become ready.
A reset or cancellation interrupts these waiting steps.

### 2.6. TTL Details (daq_service)

`daq_service` uses `--max-ttl` in seconds.
The default is `5` seconds.
`--ttl-update-interval` controls how often the plugin refreshes TTLs, with a default interval of `3` seconds.

The plugin refreshes Redis keys in two ways:

- `presence`, `fair-mq-state`, and `updatedTime` are updated with `SETEX`, which refreshes both the value and the TTL.
- `health`, `option`, topology channel keys, topology socket keys, and peer list keys are refreshed with `EXPIRE`.

```mermaid
sequenceDiagram
  participant Device as User device process<br/>(daq_service)
  participant Redis as Redis
  participant WebCtl as daq-webctl

  Device->>Redis: register service keys
  Device->>Redis: SETEX presence, fair-mq-state, updatedTime<br/>value + --max-ttl
  Device->>Redis: EXPIRE health, option, topology keys<br/>--max-ttl
  WebCtl->>Redis: SUBSCRIBE expired key events
  loop every --ttl-update-interval
    Device->>Redis: SETEX liveness keys
    Device->>Redis: EXPIRE hash/list topology keys
  end
  alt normal shutdown
    Device->>Redis: DEL registered keys
    WebCtl->>Redis: poll/scan state keys
    WebCtl-->>WebCtl: remove stopped instance from summary
  else crash or lost Redis connection
    Device-xRedis: refresh stops
    Redis-->>Redis: expire keys after --max-ttl
    Redis-->>WebCtl: expired presence key event
    WebCtl-->>WebCtl: mark instance disappeared
  end
```

On normal shutdown, the plugin deletes its registered keys.
If the process crashes or loses its Redis connection, TTL expiration removes transient registry keys after the refreshes stop.

Redis keyspace notifications are not required for TTL expiration itself.
However, `daq-webctl` needs expired-key events to detect disappeared instances without waiting for its next polling cycle.
The `metrics` plugin uses `--metrics-max-ttl` to remove fields for instances that have stopped updating their metrics, not as a Redis key TTL.
The `parameter_config` plugin does not set TTLs on parameter keys.

## 3. metrics

`metrics` records process-level metrics and FairMQ channel throughput metrics in Redis.
Process central processing unit (CPU) usage follows the top/htop convention: one fully used CPU core is approximately `100`, and two fully used cores are approximately `200`.
Memory usage is the current resident set size (RSS) in mebibytes (MiB).

<a id="31-runtime-options"></a>
### 3.1. Command-Line Options

| Option                        | Default | Description |
|-------------------------------|---------|-------------|
| `--proc-stat-update-interval` | `1000`  | Update interval in milliseconds for process CPU and memory metrics. |
| `--metrics-uri`               | none    | Redis URI for metrics. If empty, `--registry-uri` is used. |
| `--retention`                 | `0`     | RedisTimeSeries retention in milliseconds. `0` means no trimming. |
| `--recreate-ts`               | `true`  | Recreate RedisTimeSeries keys on transition to `Running`. |
| `--metrics-max-ttl`           | `3000`  | Maximum age in milliseconds since an instance's last metrics update. The plugin removes older instance fields from metric hashes. A value of zero or less disables this cleanup. |

### 3.2. Redis Keys Written or Read

| Key pattern | Redis type | Fields / value | Writer / reader | Purpose |
|-------------|------------|----------------|-----------------|---------|
| `metrics{sep}created-time` | hash | Field: `{id}`; value: creation timestamp | Written | Device creation time. |
| `metrics{sep}hostname` | hash | Field: `{id}`; value: hostname | Written | Host metadata. |
| `metrics{sep}host-ip` | hash | Field: `{id}`; value: host IP address | Written | Host metadata. |
| `metrics{sep}state` | hash | Field: `{id}`; value: FairMQ state name | Written | Current state as a string. |
| `metrics{sep}state-id` | hash | Field: `{id}`; value: numeric FairMQ state ID | Written | Current state as a numeric value. |
| `metrics{sep}last-update` | hash | Field: `{id}`; value: timestamp | Written | Last metrics update time. |
| `metrics{sep}last-update-ns` | hash | Field: `{id}`; value: timestamp in nanoseconds | Written/read | Used to identify stale metric fields. |
| `metrics{sep}cpu-stat` | hash | Field: `{id}`; value: CPU percent | Written | Process CPU usage. |
| `metrics{sep}ram-stat` | hash | Field: `{id}`; value: current RSS MiB | Written | Process memory usage. |
| `metrics{sep}msg-in`, `metrics{sep}msg-out` | hash | Field: `{id}{sep}{channel}[{subindex}]`; value: messages per second | Written | Current channel message rate. |
| `metrics{sep}mb-in`, `metrics{sep}mb-out` | hash | Field: `{id}{sep}{channel}[{subindex}]`; value: MiB per second | Written | Current channel throughput. |
| `metrics{sep}msg-in-sum`, `metrics{sep}msg-out-sum` | hash | Field: `{id}{sep}{channel}[{subindex}]`; value: cumulative rounded message count | Written | Accumulated message counts. |
| `metrics{sep}mb-in-sum`, `metrics{sep}mb-out-sum` | hash | Field: `{id}{sep}{channel}[{subindex}]`; value: cumulative MiB | Written | Accumulated throughput. |
| `metrics{sep}num-msg`, `metrics{sep}mb` | hash | Field: `{id}{sep}{channel}[{subindex}].in` or `.out`; value: current rate | Written | Direction-qualified current rates. |
| `metrics{sep}num-msg-sum`, `metrics{sep}mb-sum` | hash | Field: `{id}{sep}{channel}[{subindex}].in` or `.out`; value: cumulative value | Written | Direction-qualified cumulative values. |
| `ts{sep}{id}{sep}cpu-stat`, `ts{sep}{id}{sep}ram-stat`, `ts{sep}{id}{sep}state-id` | RedisTimeSeries | Samples added with `TS.ADD`; labels include `service`, `id`, and data type | Written | Process and state time series. |
| `ts{sep}{id}{sep}{channel}[{subindex}]{sep}...` | RedisTimeSeries | Channel rate and cumulative samples with labels such as `name`, `socket`, and `transport` | Written | Channel time series. |

The plugin listens for FairLogger throughput lines from FairMQ and parses records such as:

```text
data[0]: in: 123 (4.5 MB) out: 67 (8.9 MB)
```

Only indexed subchannel records are used for channel throughput metrics.

### 3.3. TTL and Retention Details (metrics)

`--metrics-max-ttl` is not a Redis key TTL.
It is the maximum allowed age in milliseconds of an instance's timestamp in `metrics{sep}last-update-ns`.
The plugin removes fields belonging to older instances from registered metric hashes with `HDEL`.
If `--metrics-max-ttl` is zero or negative, this cleanup is disabled.

`--retention` applies only to RedisTimeSeries keys created by the plugin.
The value is passed to `TS.CREATE ... RETENTION` in milliseconds.
A value of `0` means that RedisTimeSeries does not trim samples by retention time.

## 4. parameter_config

`parameter_config` reads Redis parameter keys and mirrors their values into FairMQ program properties.
Instance-specific parameters override group parameters when both are present.

<a id="41-runtime-options"></a>
### 4.1. Command-Line Options

| Option                   | Default | Description |
|--------------------------|---------|-------------|
| `--parameter-config-uri` | none    | Redis URI for parameter configuration. If empty, `--registry-uri` is used. |

### 4.2. Redis Keys Read or Subscribed

| Key pattern | Redis type | Fields / value | Writer / reader | Purpose |
|-------------|------------|----------------|-----------------|---------|
| `parameters{sep}{id}` | hash | Field: option name; value: option value string | Read | Instance-specific parameter set. |
| `parameters{sep}{group}` | hash | Field: option name; value: option value string | Read | Group default parameter set. `{group}` is derived from `{id}` by removing a trailing numeric `-N` suffix. |
| `parameters{sep}{id}{sep}*` | string/list/hash/set/zset | Additional structured parameters below the instance key | Read/scanned | Per-instance structured parameter values. |
| `parameters{sep}{group}{sep}*` | string/list/hash/set/zset | Additional structured parameters below the group key | Read/scanned | Group-level structured parameter values. |
| `__keyspace@{db}__:{key}` | pub/sub channel | Redis keyspace notification events | Subscribed | Triggers live reload for the instance and group parameter keys. |

String keys use the last path component as the option name.
Hash values become map-like properties, list values become array-like properties, set values become sets, and sorted-set values become maps from members to scores.

Redis keyspace notifications must be enabled on the Redis server for live reloads.
Initial parameter loading does not require keyspace notifications.

### 4.3. TTL Details (parameter_config)

`parameter_config` does not call `EXPIRE`, `SETEX`, or `DEL` for parameter keys.
It reads parameter keys and subscribes to keyspace notifications for live reloads.
If parameter keys should expire, the writer must set their TTL.
