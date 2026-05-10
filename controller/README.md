# DAQ Web Controller Implementation

This directory contains the implementation of `daq-webctl`, the NestDAQ web
controller process. It provides an HTTP server for the browser UI, WebSocket
sessions for interactive clients, and Redis-backed control operations for DAQ
devices.

The static browser assets served by `daq-webctl` are documented separately in
[`share/controller/README.md`](../share/controller/README.md).

## Runtime Role

`daq-webctl` listens on an HTTP endpoint, serves the configured document root,
and accepts WebSocket clients. Commands from the browser are translated into
Redis-backed DAQ control operations, while state updates are sent back to
connected WebSocket clients.

At startup, `daq-webctl` configures FairLogger output and can load the optional
NestDAQ OpenTelemetry plugin through the shared telemetry loader. The controller
does not link OpenTelemetry directly.

## Main Components

| Component | Purpose |
| :-- | :-- |
| `run_daq-webctl.cxx` | Executable entry point, command-line parsing, logging, telemetry, Redis setup, and server startup. |
| `HttpWebSocketServer` | Owns the Boost.Asio I/O context, signal handling, listener, and worker threads. |
| `listener` | Accepts TCP connections and starts HTTP sessions. |
| `http_session` | Handles HTTP requests and upgrades WebSocket requests. |
| `websocket_session` | Manages one WebSocket client connection. |
| `WebSocketHandle` | Dispatches JSON messages received from WebSocket clients. |
| `WebGui` | Implements Redis-backed DAQ control, state polling, and command publication. |
| `beast_tools` | Provides shared Boost.Beast HTTP response helpers. |
| `DaqWebControlDefaultDocRootPath.h.in` | Generates the default installed document root path used by `--doc-root`. |

## Typical Usage

```sh
daq-webctl --http-uri=http://0.0.0.0:8080 --redis-uri=tcp://127.0.0.1:6379
```

Open `http://localhost:8080/` or `http://localhost:8080/daq-webctl.html` after
the process starts. The Redis server and DAQ devices must be available for
control operations to succeed.

Use `daq-webctl --help` to inspect the available HTTP, Redis, FairLogger, and
OpenTelemetry options.

## Command-Line Options

`daq-webctl` accepts the following options. OpenTelemetry options are also
available through the shared NestDAQ telemetry option helper for the
`daq-webctl` component. When `--otel-service-instance-id` is not specified,
`daq-webctl` records a generated UUID in the OpenTelemetry
`service.instance.id` resource attribute.

| Option | Default | Description |
| :-- | :-- | :-- |
| `--help`, `-h` | none | Print command-line help and exit. |
| `--http-uri` | `http://0.0.0.0:8080` | HTTP server URI in `scheme://address:port` form. |
| `--threads` | `1` | Number of HTTP server worker threads. |
| `--doc-root` | installed controller document root | Directory used to serve HTML and static files. |
| `--pre-run` | `echo "pre-run command"` | Script path or command line executed before publishing `RUN`. |
| `--post-run` | `echo "post-run command"` | Script path or command line executed after publishing `RUN`. |
| `--pre-stop` | `echo "pre-stop command"` | Script path or command line executed before publishing `STOP`. |
| `--post-stop` | `echo "post-stop command"` | Script path or command line executed after publishing `STOP`. |
| `--redis-uri` | `tcp://127.0.0.1:6379` | Redis server URI. A database number can be included as `/N`. |
| `--separator` | `:` | Separator used when composing Redis key paths. |
| `--poll-interval` | `1000` | State polling interval in milliseconds. |
| `--log-to-file` | empty | FairLogger output file. If set, console logging is disabled. |
| `--file-severity` | `info` | FairLogger file severity. |
| `--severity` | `info` | FairLogger console severity. |
| `--verbosity` | `medium` | FairLogger verbosity. |
| `--color` | `true` | Enable FairLogger console colors. |

## Redis Keys

In the key patterns below, `{sep}` is the configured separator. The default is
`:`.

| Key pattern | Redis type | Read / write | Value | Purpose |
| :-- | :-- | :-- | :-- | :-- |
| `run_info{sep}run_number` | string integer | read/write/increment | Current or next run number. | Read by the UI, incremented by the UI, and copied to `latest_run_number` when `RUN` is requested. |
| `run_info{sep}latest_run_number` | string integer | read/write | Last run number copied when `RUN` was requested. | Displayed as the latest run number in the browser. |
| `run_info{sep}wait-device-ready` | string boolean | read/write | `1`, `true`, or any other string. | If true, `CONNECT` is published and the controller waits for `DeviceReady`, `Ready`, or `Running` before later commands that require device readiness. |
| `run_info{sep}wait-ready` | string boolean | read/write | `1`, `true`, or any other string. | If true, `INIT TASK` is published and the controller waits for `Ready` or `Running` before `RUN`. |
| `daq_service{sep}*{sep}*{sep}fair-mq-state` | string | read/scanned | FairMQ state name. | Polled to build service and instance state summaries. |
| `daq_service{sep}*{sep}*{sep}updatedTime` | string | read/scanned | Last update timestamp. | Polled with state keys and returned to WebSocket clients. |
| `daq_service{sep}{service}{sep}{id}{sep}presence` | string | observed by expiration event | Device UUID. | Expiration is used to notice disappearing instances. |

At startup, `daq-webctl` sets Redis `notify-keyspace-events` to `AKE` so it can
receive key-event notifications, including expired key events.

## Redis Pub/Sub

`daq-webctl` publishes DAQ control commands and subscribes to state and key-event
channels.

| Channel | Direction | Message | Purpose |
| :-- | :-- | :-- | :-- |
| `daqctl` | publish | JSON command object | Sends DAQ state-transition commands to devices using the `daq_service` plugin. |
| `daqstate` | subscribe | JSON state message | Receives DAQ state-transition notifications. The current implementation validates that a `value` field exists. |
| `__keyevent@{db}__:expired` | subscribe | Expired Redis key name | Detects expired `presence` keys and updates connected clients. |

Messages published to `daqctl` have this shape:

```json
{
  "command": "change_state",
  "value": "RUN",
  "services": ["Sampler", "Sink"],
  "instances": ["Sampler-0", "Sink-0"]
}
```

The `value` field can be one of the FairMQ or NestDAQ command strings handled by
the controller:

```text
BIND, COMPLETE INIT, CONNECT, END, INIT DEVICE, INIT TASK, RESET DEVICE,
RESET TASK, RUN, STOP, exit, quit, reset, start
```

When `RUN` is requested, `daq-webctl` copies `run_info{sep}run_number` to
`run_info{sep}latest_run_number`, optionally publishes prerequisite `CONNECT`
and `INIT TASK` commands according to the wait flags, runs `--pre-run`,
publishes `RUN`, and then runs `--post-run`.

When `STOP` is requested, `daq-webctl` runs `--pre-stop`, publishes `STOP`, and
then runs `--post-stop`.

## WebSocket Messages

Browser clients send JSON commands to the WebSocket endpoint. The controller
executes Redis operations or publishes Redis pub/sub messages.

| Client message | Effect |
| :-- | :-- |
| `{"command":"redis-get","value":"run_number"}` | Reads `run_info{sep}run_number` and `run_info{sep}latest_run_number`. |
| `{"command":"redis-incr","value":"run_number"}` | Increments `run_info{sep}run_number`. |
| `{"command":"redis-set","name":"wait-ready","value":"true"}` | Sets one of the known `run_info` values. Valid names are `run_number`, `wait-device-ready`, and `wait-ready`. |
| `{"command":"redis-publish","value":"RUN","services":["Sampler"],"instances":["Sampler-0"]}` | Publishes a DAQ command to `daqctl`, with optional prerequisite command handling. |

The controller sends JSON messages back to browser clients.

| Controller message | Meaning |
| :-- | :-- |
| `{"type":"set run_number","value":"..."}` | Updated run number. |
| `{"type":"set latest_run_number","value":"..."}` | Updated latest run number. |
| `{"type":"error","value":"..."}` | Redis read or command handling error. |
| `{"type":"state-summary-table", ...}` | Full service/instance state summary. |

The `state-summary-table` message contains:

- `service_list_changed`: true when the set of services changed.
- `instance_list_changed`: true when the set of instances changed.
- `services`: array of service summaries.
- per-service `counts`: array of FairMQ state counters.
- per-service `instances`: array with `service`, `instance`, `state`, and
  `date`.

## State Polling and Expiration

`daq-webctl` polls `daq_service{sep}*{sep}*{sep}fair-mq-state` and
`daq_service{sep}*{sep}*{sep}updatedTime` every `--poll-interval` milliseconds.
The resulting summary is broadcast to all connected WebSocket clients.

Redis expired key events are processed separately. When a `presence` key expires,
the controller derives the service and instance from the key name and updates
connected clients so the UI can reflect disappeared instances.
