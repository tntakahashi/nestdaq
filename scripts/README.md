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

The generated script sends OpenTelemetry (OTel) logs to a local OpenTelemetry
Collector with OpenTelemetry Protocol (OTLP) gRPC. The default endpoint is
`localhost:4317` and can be changed with `NESTDAQ_OTLP_GRPC_ENDPOINT`.

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

## Device skeleton generation

`generate-device-skeleton.py` creates a minimal NestDAQ FairMQ device project
from the templates installed under `share/device-skeleton`.

```bash
./generate-device-skeleton.py MyDevice --output ./MyDevice
```

The generated project contains `MyDevice.h`, `MyDevice.cxx`,
`CMakeLists.txt`, and `README.md`. Existing files are not overwritten unless
`--force` is specified. Use `--dry-run` to inspect the output paths without
writing files.

The generator reads the `*.in` template files from `share/device-skeleton`,
substitutes the device-specific placeholders, and writes the resulting files to
the output directory. The main substitutions are `@CLASS_NAME@`,
`@HEADER_FILE@`, and `@SOURCE_FILE@`.

| Template | Generated file for `MyDevice` |
| :-- | :-- |
| `Device.h.in` | `MyDevice.h` |
| `Device.cxx.in` | `MyDevice.cxx` |
| `CMakeLists.txt.in` | `CMakeLists.txt` |
| `README.md.in` | `README.md` |

Build the generated device as a standalone CMake project. Set
`CMAKE_PREFIX_PATH` to the NestDAQ install prefix.

```bash
cmake -S ./MyDevice -B ./build-MyDevice -G Ninja \
  -DCMAKE_PREFIX_PATH=<nestdaq-install-prefix>
cmake --build ./build-MyDevice --parallel
```

The skeleton is intentionally minimal. Use the `Sampler` and `Sink` examples
for data-channel handling and telemetry instrumentation examples.

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

### topology-1-1.sh
A simple topology of **Sampler** and **Sink** with the **PUSH-PULL** pattern. 
If _N_ Sasmplers and _N_ Sinks are started, they forms _N_ pairs of Sampler and Sink.  
Each Sampler sends data to one Sink with the same instance index. 

```bash
  ./topology-1-1.sh
```

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

```mermaid
graph LR
  Sampler-0 --> fairmq-splitter-0
  Sampler-1 --> fairmq-splitter-1
  Sampler-2 --> fairmq-splitter-2
  fairmq-splitter-0 & fairmq-splitter-1 & fairmq-splitter-2  --> Sink-0 & Sink-1
```

### topology-2samplers-n-m.sh
Two sampler services send data to one sink service. 

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
