# Scripts

Set of examples of how to use the plugins.
The scripts can be copied to your favorite directory. 
Redis server must be started before executing the scripts. 

## Helper script to launch a data acquisition (DAQ) process

### start_device.sh 
This example shows how to start FairMQDevice with the custom plugins. 
The device must be those provided by the present repository or those which contains `fairmq-` in the path. 
Arguments after the device name are passed through to the device and FairMQ, so
plugin options such as `--service-name` and device-specific options such as
`--max-iterations` can be specified on the same command line.

For a typical local validation run, prepare the runtime environment before
starting devices with `start_device.sh`:

- Start a Redis server.
- Start an OpenTelemetry Collector backend, for example the Compose setup under
  `share/otel-collector-compose`.
- Start `daq-webctl` if you want to control devices from the browser user
  interface.
- Register topology settings in Redis with a `topology-*.sh` script.
- Register parameter settings in Redis with `mq-param.sh` when the examples
  should read parameters from the `parameter_config` plugin.

See [`examples/README.md`](../examples/README.md) for the full local run
sequence.

The generated script uses `NESTDAQ_REDIS_SERVER` for all NestDAQ Redis
connections. The default is `127.0.0.1:6379`. It maps the DAQ service registry
to Redis database `0`, metrics to database `1`, and parameter configuration to
database `2`.

The relevant part of the script is:

```bash
NESTDAQ_REDIS_SERVER=${NESTDAQ_REDIS_SERVER:-127.0.0.1:6379}

DAQSERVICE_URI=" --registry-uri tcp://${NESTDAQ_REDIS_SERVER}/0"
METRICS_URI=" --metrics-uri tcp://${NESTDAQ_REDIS_SERVER}/1"
CONFIG_URI=" --parameter-config-uri tcp://${NESTDAQ_REDIS_SERVER}/2"
```

`daq_service` uses DB 0 for the service registry, DAQ commands, and topology
metadata. The `metrics` plugin uses DB 1. The `parameter_config` plugin reads
device option values from DB 2.

The script also sets the plugin search path and the plugin load order:

```bash
PLUGIN_SEARCH_PATH=" -S '<$PLUGIN_LIBDIR'"
DAQSERVICE_PLUGIN=" -P daq_service"
METRICS_PLUGIN=" -P metrics"
CONFIG_PLUGIN=" -P parameter_config"

var+=$PLUGIN_SEARCH_PATH
var+=$DAQSERVICE_PLUGIN
var+=$METRICS_PLUGIN
var+=$CONFIG_PLUGIN
```

`-S` adds a directory to the FairMQ plugin search path. In this script,
`-S '<$PLUGIN_LIBDIR'` prepends the installed NestDAQ plugin directory to that
search path. It only controls where plugin libraries are searched.

`-P` selects a plugin to load. The plugin load order follows the order of the
`-P` options on the final command line. The generated `start_device.sh` passes
them as `daq_service`, then `metrics`, then `parameter_config`. Adding more
directories after `-S` changes search priority, but it does not change which
plugins are loaded or their load order; that is controlled by the `-P` entries.

The generated script sends OpenTelemetry (OTel) logs to a local OpenTelemetry
Collector with OpenTelemetry Protocol (OTLP) gRPC. The default endpoint is
`localhost:4317` and can be changed with `NESTDAQ_OTLP_GRPC_ENDPOINT`.

The script builds the OTel log options like this:

```bash
NESTDAQ_OTLP_GRPC_ENDPOINT=${NESTDAQ_OTLP_GRPC_ENDPOINT:-localhost:4317}
NESTDAQ_START_DEVICE_OTEL_LOG_SEVERITY=${NESTDAQ_START_DEVICE_OTEL_LOG_SEVERITY:-info}

var+=" --otel-log-protocol=otlp-grpc"
var+=" --otel-log-endpoint-grpc=${NESTDAQ_OTLP_GRPC_ENDPOINT}"
var+=" --otel-log-severity=${NESTDAQ_START_DEVICE_OTEL_LOG_SEVERITY}"
```

Choose the endpoint according to where the process runs:

- Host process to a compose-published collector port: `localhost:4317`.
- NestDAQ device container or `daq-webctl` container in the same OpenSearch or
  Victoria compose network: `otel-collector:4317`.
- NestDAQ device container or `daq-webctl` container in the same ClickStack
  compose network: `clickstack:4317`.
- Container outside the compose network to the host-published collector port:
  Docker commonly uses `host.docker.internal:4317`; Podman commonly uses
  `host.containers.internal:4317`.

```bash
NESTDAQ_OTLP_GRPC_ENDPOINT=host.containers.internal:4317 ./start_device.sh Sampler
```

OTel metrics and traces are disabled by default. Uncomment the metric and trace
examples in `start_device.sh` to export them by OTLP gRPC or to print them to
the console exporter for debugging.

FairLogger console output is disabled by default with `--severity nolog`.
Change `NESTDAQ_FAIRLOGGER_CONSOLE_SEVERITY` to enable it. OTel log export uses
the separate `NESTDAQ_START_DEVICE_OTEL_LOG_SEVERITY` threshold and still sends
logs to the collector when FairLogger console output is disabled.

```bash
NESTDAQ_FAIRLOGGER_CONSOLE_SEVERITY=${NESTDAQ_FAIRLOGGER_CONSOLE_SEVERITY:-nolog}

var+=" --severity ${NESTDAQ_FAIRLOGGER_CONSOLE_SEVERITY}"
```

```bash
NESTDAQ_FAIRLOGGER_CONSOLE_SEVERITY=debug4 NESTDAQ_START_DEVICE_OTEL_LOG_SEVERITY=debug4 ./start_device.sh Sampler
```

```bash
  # ./start_device.sh [device-name] [options ...]
  ./start_device.sh Sampler
```

```bash
  ./start_device.sh /your-fairmq-install-path/bin/fairmq-splitter
```

An example of launching a `Sampler` with a different service name (`A-Sampler`) and limiting the execution rate of `ConditionalRun()` to once per second. 
```bash
./start_device.sh Sampler --service-name A-Sampler --rate 1
```

`start_device.sh` does not set `--service-name` by itself. Options after the
device name are passed through to FairMQ and the NestDAQ plugins. See
[`plugins/README.md#daq-service-identity-defaults`](../plugins/README.md#daq-service-identity-defaults)
for the `daq_service` defaults used when `--service-name` or `--id` is empty.

```bash
./start_device.sh Sampler --service-name A-Sampler
./start_device.sh Sampler --service-name B-Sampler
```

This lets the same executable appear as separate service groups. For example,
the same `Sampler` program can appear as `A-Sampler-*` and `B-Sampler-*` in
Redis, `daq-webctl`, and telemetry attributes.

```mermaid
flowchart TB
  subgraph Program["Same executable: Sampler"]
    direction LR

    subgraph A["service-name: A-Sampler"]
      direction TB
      A0["A-Sampler-0"]
      A1["A-Sampler-1"]
    end

    subgraph B["service-name: B-Sampler"]
      direction TB
      B0["B-Sampler-0"]
      B1["B-Sampler-1"]
    end
  end
```

## Topology configuration

Default value for endpoint parameter

| field                 | default value                              | 
| --                    | --                                         | 
| name                  |                                            | 
| type                  |                                            | 
| method                |                                            | 
| address               |                                            | 
| transport             | zeromq                                     |
| sndBufSize            | 1000                                       | 
| rcvBufSize            | 1000                                       | 
| sndKernelSize         | 0                                          |
| linger                | 500                                        |
| rateLogging           | 1                                          |
| portRangeMin          | 22000                                      |
| portRangeMax          | 32000                                      |
| autoBind              | true                                       |
| numSockets            | 0 (Automatically calculated by the plugin) |
| autoSubChannel        | false                                      |
| bound                 | (Do not set by the user)                   |
| waitForPeerConnection | true                                       | 

The last three parameters are specific to nestdaq.
The rest are defined in FairMQ.

`autoSubChannel` controls whether a peer written without `[subindex]` means
only subchannel `0` or all subchannels registered for that peer channel.
Use `autoSubChannel false` for fixed 1:1-style connections such as
`topology-1-1.sh`. Use `autoSubChannel true` for n:m-style fan-out or fan-in
topologies such as `topology-n-n-m.sh` and `topology-2samplers-n-m.sh`, where
the plugin discovers peer subchannels and updates `numSockets` accordingly.
When `[subindex]` is written explicitly, only that subchannel is used.
See [`plugins/README.md#autosubchannel`](../plugins/README.md#autosubchannel)
for the detailed topology plugin behavior.

Topology scripts write endpoint and link definitions to Redis DB 0. Their
helper functions have this shape:

```bash
server=redis://127.0.0.1:6379/0

function endpoint () {
  redis-cli -u $server hset daq_service:topology:endpoint:$1:$2 ${@:3}
}

function link () {
  redis-cli -u $server set daq_service:topology:link:$1:$2,$3:$4 none
}
```

`endpoint SERVICE CHANNEL ...` writes a hash at
`daq_service:topology:endpoint:SERVICE:CHANNEL`. The remaining fields describe
the FairMQ socket, for example `type push`, `method bind`, and
`autoSubChannel false`.

### Bind and connect endpoints

In topology endpoint settings, `method bind` and `method connect` describe
which side owns the socket address. Here, an address means the endpoint
connection information needed by FairMQ: an IP address or hostname plus a port
number. A bind-side socket opens its local endpoint and can communicate with
connect-side sockets that connect to it without knowing each peer address. A
connect-side socket must know the bind-side address before it can connect. That
address can be resolved from NestDAQ service discovery and topology metadata in
Redis, or it can be set directly through parameters for a fixed setup.

`link SERVICE CHANNEL PEER_SERVICE PEER_CHANNEL` writes a logical connection
between two endpoint definitions. The topology plugin reads these definitions
when each device starts and turns them into concrete FairMQ channel properties.

### topology-1-1.sh
A simple topology of **Sampler** and **Sink** with the **PUSH-PULL** pattern. 
If _N_ Samplers and _N_ Sinks are started, they form _N_ pairs of Sampler and Sink.
Each Sampler sends data to one Sink with the same instance index. 

```bash
  ./topology-1-1.sh
```

The key lines are:

```bash
endpoint Sampler data type push method bind autoSubChannel false
endpoint Sink    in   type pull method connect autoSubChannel false

link Sampler data Sink in
```

`Sampler:data` binds a PUSH socket, `Sink:in` connects a PULL socket, and the
link pairs devices with matching instance indexes such as `Sampler-0` to
`Sink-0`.

```mermaid
graph LR
  Sampler-0 --> Sink-0
  Sampler-1 --> Sink-1
  Sampler-2 --> Sink-2
```

### topology-n-n-m.sh
A simple topology of _N_-**Sampler**s, _N_-**fairmq-splitter**s, and _M_-**Sink**s with the **PUSH-PULL** pattern. 
Each Sampler sends data to one fairmq-splitter with the same instance index. 
Then, the fairmq-splitter sends the data to Sinks. 
The `autoSubChannel true` flag is used to give each sub-socket a different `address:port` and to distinguish them by index.
The fairmq-splitter determines the destination by the number of messages sent in a round-robin fashion.

```bash
  ./topology-n-n-m.sh
```

The splitter topology uses two channels on `fairmq-splitter`:

```bash
endpoint Sampler          data     type push method bind    autoSubChannel false
endpoint fairmq-splitter  data-in  type pull method connect autoSubChannel false
endpoint fairmq-splitter  data-out type push method bind    autoSubChannel true
endpoint Sink             in       type pull method connect autoSubChannel true

link Sampler         data     fairmq-splitter data-in
link fairmq-splitter data-out Sink            in
```

The first link keeps each sampler paired with the splitter instance of the same
index. The second link uses `autoSubChannel true` so splitter output
subchannels can fan out to multiple sink instances.

```mermaid
graph LR
  Sampler-0 --> fairmq-splitter-0
  Sampler-1 --> fairmq-splitter-1
  Sampler-2 --> fairmq-splitter-2
  fairmq-splitter-0 & fairmq-splitter-1 & fairmq-splitter-2  --> Sink-0 & Sink-1
```

### topology-2samplers-n-m.sh
Two sampler services send data to one sink service. 

This script demonstrates the service-name grouping described above. It expects
some `Sampler` processes to be started as `A-Sampler` and others as
`B-Sampler`:

```bash
endpoint A-Sampler data type push method bind autoSubChannel true
endpoint B-Sampler data type push method bind autoSubChannel true
endpoint Sink      in   type pull method connect autoSubChannel true

link A-Sampler data Sink in
link B-Sampler data Sink in
```

```mermaid
graph LR
  A-Sampler-0 & A-Sampler-1 & B-Sampler-0 & B-Sampler-1 & B-Sampler-2 --> Sink-0 & Sink-1 
```

## Parameter configuration

### mq-param.sh
This example shows how to configure parameters via Redis. 

```bash
  ./mq-param.sh
```

`mq-param.sh` writes parameter hashes to Redis DB 2, which is the database used
by the `parameter_config` plugin:

```bash
server=redis://127.0.0.1:6379/2

function param () {
  redis-cli -u $server hset parameters:$1 ${@:2}
}
```

The first argument is the instance id. The rest are field/value pairs that
become FairMQ or device options for that instance:

```bash
param Sampler-0 text Hello rate 2 max-iterations 0
param Sampler-1 text world rate 2 max-iterations 0

param Sink-0 multipart true
param Sink-1 multipart true
```

For example, `param Sampler-0 text Hello rate 2 max-iterations 0` writes a hash
named `parameters:Sampler-0` with fields `text`, `rate`, and `max-iterations`.
When `Sampler-0` starts with the `parameter_config` plugin, those values are
mirrored into the device program options.

## Device skeleton generation

`generate-device-skeleton.py` creates a minimal NestDAQ FairMQ device project
from templates built into the script.

```bash
./generate-device-skeleton.py MyDevice --output ./MyDevice
```

The generated project contains `MyDevice.h`, `MyDevice.cxx`,
`CMakeLists.txt`, and `README.md` unless those helper files are omitted by
options. Existing files are not overwritten unless `--force` is specified. Use
`--dry-run` to inspect the output paths without writing files. Use
`--no-cmake` when the device will be added to an existing build system and
`CMakeLists.txt` should not be generated. Use `--no-readme` when the generated
device does not need its own `README.md`.

Generator options:

| Option | Default | Description |
| :-- | :-- | :-- |
| `--output DIR`, `-o DIR` | `./CLASS_NAME` | Write generated files under `DIR`. |
| `--force` | off | Overwrite existing generated files. |
| `--dry-run` | off | Print the files that would be generated without writing them. |
| `--interactive` | off | Prompt for generation choices instead of specifying all options on the command line. |
| `--no-cmake` | off | Do not generate `CMakeLists.txt`; use this when integrating the device into an existing build system. |
| `--no-readme` | off | Do not generate `README.md`; use this when the generated device will be documented elsewhere. |
| `--no-namespace` | off | Generate the device class in the global namespace instead of `namespace nestdaq`. |
| `--processing-mode MODE` | `conditional-run` | Select the generated processing entry point: `conditional-run`, `run`, or `on-data`. |
| `--input-channel SPEC` | none | Generate input-channel code. `SPEC` is `KEY:DEFAULT_NAME`, `:DEFAULT_NAME`, or `DEFAULT_NAME`. |
| `--output-channel SPEC` | none | Generate output-channel code. `SPEC` uses the same format as `--input-channel`. |
| `--dqm-channel SPEC` | none | Generate data quality monitor (DQM) channel code. `SPEC` uses the same format as `--input-channel`. |
| `--multipart-input` | off | Generate multipart receive/`OnData()` examples for the input channel. Requires `--input-channel`. |
| `--single-output` | off | Generate single-message output examples. Output is multipart by default. |
| `--single-dqm` | off | Generate single-message DQM examples. DQM is multipart by default. |
| `--no-drain-input` | off | Do not generate `PostRun()` input drain code. |
| `--no-poll LIST` | none | Comma-separated channel kinds to exclude from FairMQ polling: `input`, `output`, `dqm`. |

Processing modes:

| Mode | Generated behavior |
| :-- | :-- |
| `conditional-run` | Generates `ConditionalRun()` with simple poll/receive/send examples. |
| `run` | Generates an empty `Run()`. |
| `on-data` | Generates an `OnData()` callback registration in `InitTask()`; requires `--input-channel`. |

For how to choose between `OnData()`, `ConditionalRun()`, and `Run()`, see
[`examples/README.md#choosing-ondata-conditionalrun-or-run`](../examples/README.md#choosing-ondata-conditionalrun-or-run).

Channel options passed to the generator are not the final device command-line
options. They describe how to generate those options in C++:

```bash
./generate-device-skeleton.py MyProcessor \
  --input-channel in-chan-name:in \
  --output-channel out-chan-name:data \
  --dqm-channel dqm-chan-name:dqm
```

For example, `--input-channel in-chan-name:in` makes the generated C++ add an
`in-chan-name` command-line option whose default value is `in`, then read that
option into `fInputChannelName` in `InitTask()`. The short forms
`--input-channel :in` and `--input-channel in` both use the default option key
`in-chan-name`; output and DQM use `out-chan-name` and `dqm-chan-name` in the
same way. `KEY:` and `:` are rejected because the generated option would have
no default channel name.

The generated device class is placed in `namespace nestdaq` by default.
Use `--no-namespace` to generate the class in the global namespace.

Useful variants:

```bash
./generate-device-skeleton.py MySource \
  --output-channel out-chan-name:data

./generate-device-skeleton.py MyShortFormProcessor \
  --input-channel :in \
  --output-channel data \
  --dqm-channel dqm

./generate-device-skeleton.py MySingleMessageProcessor \
  --input-channel :in \
  --output-channel data \
  --dqm-channel dqm \
  --single-output \
  --single-dqm

./generate-device-skeleton.py MySink \
  --processing-mode on-data \
  --input-channel in-chan-name:in

./generate-device-skeleton.py MyMultipartSink \
  --processing-mode on-data \
  --input-channel in-chan-name:in \
  --multipart-input

./generate-device-skeleton.py MyDevice \
  --input-channel in-chan-name:in \
  --output-channel out-chan-name:data \
  --no-poll output,dqm \
  --no-drain-input

./generate-device-skeleton.py MyGlobalDevice \
  --no-namespace

./generate-device-skeleton.py MyIntegratedDevice \
  --no-cmake

./generate-device-skeleton.py MyNoReadmeDevice \
  --no-readme

./generate-device-skeleton.py --interactive
```

When input polling is generated, the skeleton uses a FairMQ poller before
`Receive()`. When output or DQM polling is generated, it uses
`Poller::CheckOutput()` before `Send()`. Output waits in poll-timeout steps
until it can send or a state transition is pending. DQM drops the sample if it
cannot send immediately. Output and DQM examples are generated as multipart
messages by default. Use generator options `--single-output` or `--single-dqm`
to generate single-message examples instead. `SendOutputMessage()` and
`SendDQMMessage()` take the generated `fair::mq::Parts&` or `fair::mq::MessagePtr&`
payload and only handle channel readiness, `Send()`, and success/failure
checks.

The options in the table above are generator options, not runtime command-line
options of the generated device. The generated C++ custom options are
registered as strings. Numeric members are assigned in `InitTask()` by
converting those strings:

| Generated runtime option | Default | Description |
| :-- | :-- | :-- |
| `poll-timeout-ms` | `100` | FairMQ poll timeout in milliseconds. |
| `drain-timeout-ms` | `100` | Receive timeout used by input drain. Negative values are treated as `0`. |
| `drain-max-timeout-count` | `20` | Stop input drain after this many consecutive receive timeouts since the last drained message; must be positive. |

Input drain code is generated in `PostRun()` by default when an input channel
is present; disable it with `--no-drain-input`.

The generator reads built-in templates, substitutes the device-specific
placeholders, and writes the resulting files to the output directory. The main
substitutions include `@CLASS_NAME@`,
`@HEADER_FILE@`, `@SOURCE_FILE@`, and generated C++ blocks for members,
options, processing methods, send helpers, and drain code.

| Template | Generated file for `MyDevice` |
| :-- | :-- |
| `Device.h.in` | `MyDevice.h` |
| `Device.cxx.in` | `MyDevice.cxx` |
| `CMakeLists.txt.in` | `CMakeLists.txt` |
| `README.md.in` | `README.md` |

`CMakeLists.txt` is omitted when `--no-cmake` is specified. `README.md` is
omitted when `--no-readme` is specified.

When `CMakeLists.txt` is generated, build the generated device as a standalone
CMake project. Set
`CMAKE_PREFIX_PATH` to the NestDAQ install prefix, and set
`CMAKE_INSTALL_PREFIX` to the install prefix for the generated device. These
prefixes may be the same directory. The generated CMake project uses C++17 by
default and rejects standards older than C++17.

```bash
cmake -S ./MyDevice -B ./build-MyDevice \
  -DCMAKE_PREFIX_PATH=<nestdaq-install-prefix> \
  -DCMAKE_INSTALL_PREFIX=<device-install-prefix>
cmake --build ./build-MyDevice --parallel
cmake --install ./build-MyDevice
```

The installed executable is placed under `<device-install-prefix>/bin/MyDevice`.

The skeleton is intentionally minimal. Use the `Sampler` and `Sink` examples
for data-channel handling and telemetry instrumentation examples.
