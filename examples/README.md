# Examples

This directory contains small NestDAQ device examples. The examples are a
standalone CMake project, and are also included in the main NestDAQ build when
`NestDAQ_BUILD_EXAMPLES=ON` is set. `NestDAQ_BUILD_EXAMPLES` defaults to `ON`.

## Example Devices

| Executable | Purpose |
| :-- | :-- |
| `NullDevice` | Minimal FairMQ device that exercises the NestDAQ `runDevice.h` entry point and lifecycle hooks without data channels. |
| `Sampler` | Sends text messages through an output channel and demonstrates custom command-line options, spans, and metrics. |
| `Sink` | Receives single-part or multipart messages through an input channel and demonstrates channel callback setup, spans, and metrics. |

Each executable links to `NestDAQ::NestDAQ`, which provides the NestDAQ
`runDevice.h` integration, FairMQ/FairLogger dependencies, plugin search paths,
and optional telemetry loader support.

`Sampler` and `Sink` use the NestDAQ telemetry facade to demonstrate trace spans
and metrics without including OpenTelemetry headers. Enable them at runtime with
the telemetry options, for example `--otel-metric-protocol=console` and
`--otel-trace-protocol=console`.

## Build

The main NestDAQ build builds and installs these examples by default. Configure
with `-DNestDAQ_BUILD_EXAMPLES=OFF` to skip them.

For a separate examples build, install NestDAQ first, then configure the
examples with the NestDAQ install prefix in `CMAKE_PREFIX_PATH`.

```sh
cmake \
  -DCMAKE_PREFIX_PATH=<nestdaq-install-prefix> \
  -DCMAKE_INSTALL_PREFIX=<examples-install-prefix> \
  -B ./build-examples \
  -S ./examples
cmake --build ./build-examples --parallel
cmake --install ./build-examples
```

The examples do not need to be installed into the same prefix as NestDAQ, but
the runtime linker must be able to find NestDAQ, FairMQ, Boost, and related
libraries. The example CMake project sets an install runtime search path
(rpath) relative to the
example install prefix and uses link paths discovered through `NestDAQ::NestDAQ`.

## Running

Use the installed helper scripts or invoke the binaries directly with FairMQ
channel options. A typical local validation run starts Redis, an OpenTelemetry
Collector backend, `daq-webctl`, and then the example devices.

```sh
Sampler --help
Sink --help
NullDevice --help
```

### Local Run Sequence

The commands below assume that NestDAQ was installed under
`<install-prefix>`. Run long-lived processes in separate terminals.

1. Start Redis.

   Redis is required by the NestDAQ DAQ service, metrics, and parameter
   configuration plugins. One local option is the Redis Stack Server container
   helper:

   ```sh
   cp -a <install-prefix>/share/redis-stack-container ./redis-stack-container
   cd ./redis-stack-container
   ./run-redis-stack-server.sh
   ```

   See
   [`share/redis-stack-container/README.md`](../share/redis-stack-container/README.md)
   for Docker, Podman, volume, and RedisInsight options.

2. Start an OpenTelemetry Collector backend.

   This example uses the OpenSearch backend. It receives OpenTelemetry Protocol
   (OTLP) data from the example devices, stores logs and traces in OpenSearch,
   and makes them available in OpenSearch Dashboards.

   ```sh
   cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
   cd ./otel-collector-compose/opensearch
   docker compose -f compose-opensearch.yaml up
   ```

   For Podman, use the same file with `podman compose`. See
   [`share/otel-collector-compose/opensearch/README.md`](../share/otel-collector-compose/opensearch/README.md)
   for ports, rootless Podman notes, and dashboard setup details. The default
   OTLP gRPC endpoint is `localhost:4317`.

3. Start `daq-webctl`.

   ```sh
   <install-prefix>/bin/daq-webctl \
     --http-uri=http://0.0.0.0:8080 \
     --redis-uri=tcp://127.0.0.1:6379 \
     --otel-log-protocol=otlp-grpc \
     --otel-log-endpoint-grpc=localhost:4317 \
     --otel-log-severity=info \
     --otel-service-name=daq-webctl
   ```

   Open `http://localhost:8080/` after the process starts. The OpenTelemetry
   options send controller logs to the local collector started above. See
   [`controller/README.md`](../controller/README.md) for controller options and
   Redis command behavior, and
   [`nestdaq/telemetry/README.md`](../nestdaq/telemetry/README.md) for the full
   telemetry option list.

4. Run the example devices with `start_device.sh`.

   The installed script loads the NestDAQ plugins, uses Redis at
   `127.0.0.1:6379` by default, and exports OpenTelemetry logs to the local
   collector by OTLP gRPC. Metrics and traces are disabled by default in the
   script; see [`scripts/README.md`](../scripts/README.md) to enable them or to
   print telemetry to the console.

   `NullDevice` has no data channel and can be started directly:

   ```sh
   <install-prefix>/scripts/start_device.sh NullDevice
   ```

   `Sampler` and `Sink` need matching channel configuration. For the simple
   one-to-one topology, configure Redis first:

   ```sh
   cd <install-prefix>/scripts
   ./topology-1-1.sh
   ```

   Then start `Sink` and `Sampler` in separate terminals. Starting `Sink` first
   avoids dropping early messages while the receiver is not yet connected.

   ```sh
   <install-prefix>/scripts/start_device.sh Sink
   ```

   ```sh
   <install-prefix>/scripts/start_device.sh Sampler
   ```

   The browser controller can then publish DAQ commands such as `INIT`,
   `BIND`, `CONNECT`, `RUN`, and `STOP` to the running devices.

### Example-Specific Options

The examples also accept FairMQ options, NestDAQ plugin options, and NestDAQ
telemetry options. Use `--help` on each executable for the complete option set.

| Executable | Option | Default | Description |
| :-- | :-- | :-- | :-- |
| `Sampler` | `--out-chan-name` | `data` | Output channel name used by the producer. |
| `Sampler` | `--text` | `Hello` | Text payload prefix sent in each message. |
| `Sampler` | `--max-iterations` | `0` | Maximum number of run-loop iterations. `0` means infinite. |
| `Sink` | `--in-chan-name` | `in` | Input channel name used by the consumer. |
| `Sink` | `--multipart` | `true` | Handle incoming data as multipart messages. |

For script-based launch examples, see [`scripts/README.md`](../scripts/README.md).
