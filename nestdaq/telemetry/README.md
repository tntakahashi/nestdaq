# Telemetry

NestDAQ telemetry is an optional OpenTelemetry integration for FairMQ-based
devices and controller processes. The application executable does not link
OpenTelemetry directly. Instead, NestDAQ loads a single runtime plugin,
`libnestdaq_otel.so`, with `dlopen()` and resolves a small C ABI.

The plugin can export three OpenTelemetry signals:

| Signal  | Default            | Source in NestDAQ                                      |
| ------- | ------------------ | ----------------------------------------------------- |
| Logs    | `console` exporter | FairLogger custom sink                                |
| Metrics | disabled           | `nestdaq::telemetry::Telemetry` counter/histogram/gauge API |
| Traces  | disabled           | `nestdaq::telemetry::TelemetrySpan` RAII API          |

`libnestdaq_otel.so` is built and installed only when `opentelemetry-cpp` is
found at CMake configure time.

## Runtime Model

NestDAQ installs process-wide OpenTelemetry providers inside the telemetry
plugin. FairLogger logs are captured by a process-wide custom sink. Metrics and
traces are recorded through the NestDAQ thin wrapper API, which does not expose
OpenTelemetry C++ headers.

The runtime plugin keeps the public C ABI in `OpenTelemetryInitializer.cxx` and
organizes the implementation internally by signal area: logs, metrics, traces,
and shared runtime helpers. Applications should use `TelemetryLibrary`,
`Telemetry`, `Counter`, `Histogram`, `Gauge`, `TelemetrySpan`, and
`GetTelemetry()` instead of depending on those internal implementation files.

Each signal accepts a comma-separated protocol list. Supported protocols are
`console`, `otlp-http`, and `otlp-grpc`; the aliases `http`, `otlp_http`, `grpc`,
and `otlp_grpc` are also accepted by the plugin. An empty protocol disables the
signal.

## Command-Line Options

| Option | Env var | Default | Meaning |
| ------ | ------- | ------- | ------- |
| `--otel-library` | `NESTDAQ_OTEL_LIBRARY` | `libnestdaq_otel.so` | Shared library path or soname loaded with `dlopen()`. |
| `--otel-log-protocol` | `NESTDAQ_OTEL_LOG_PROTOCOL` | `console` | Comma-separated log exporters; empty disables logs. |
| `--otel-metric-protocol` | `NESTDAQ_OTEL_METRIC_PROTOCOL` | empty | Comma-separated metric exporters; empty disables metrics. |
| `--otel-trace-protocol` | `NESTDAQ_OTEL_TRACE_PROTOCOL` | empty | Comma-separated trace exporters; empty disables traces. |
| `--otel-log-endpoint-http` | `NESTDAQ_OTEL_LOG_ENDPOINT_HTTP` | `http://localhost:4318/v1/logs` | OTLP HTTP logs endpoint. |
| `--otel-log-endpoint-grpc` | `NESTDAQ_OTEL_LOG_ENDPOINT_GRPC` | `localhost:4317` | OTLP gRPC logs endpoint. |
| `--otel-metric-endpoint-http` | `NESTDAQ_OTEL_METRIC_ENDPOINT_HTTP` | `http://localhost:4318/v1/metrics` | OTLP HTTP metrics endpoint. |
| `--otel-metric-endpoint-grpc` | `NESTDAQ_OTEL_METRIC_ENDPOINT_GRPC` | `localhost:4317` | OTLP gRPC metrics endpoint. |
| `--otel-trace-endpoint-http` | `NESTDAQ_OTEL_TRACE_ENDPOINT_HTTP` | `http://localhost:4318/v1/traces` | OTLP HTTP traces endpoint. |
| `--otel-trace-endpoint-grpc` | `NESTDAQ_OTEL_TRACE_ENDPOINT_GRPC` | `localhost:4317` | OTLP gRPC traces endpoint. |
| `--otel-log-headers` | `NESTDAQ_OTEL_LOG_HEADERS` | empty | Comma-separated `key=value` log exporter headers. |
| `--otel-metric-headers` | `NESTDAQ_OTEL_METRIC_HEADERS` | empty | Comma-separated `key=value` metric exporter headers. |
| `--otel-trace-headers` | `NESTDAQ_OTEL_TRACE_HEADERS` | empty | Comma-separated `key=value` trace exporter headers. |
| `--otel-log-severity` | `NESTDAQ_OTEL_LOG_SEVERITY` | `info` | Minimum FairLogger severity exported. |
| `--otel-log-required` | `NESTDAQ_OTEL_LOG_REQUIRED` | `false` | Fail startup if telemetry cannot load or initialize. |
| `--otel-timeout-ms` | none | `5000` | Force-flush, shutdown, and exporter timeout in milliseconds. |
| `--otel-metric-export-interval-ms` | none | `1000` | Periodic metric export interval in milliseconds. |
| `--otel-log-http-json` | none | `true` | Use JSON content type for OTLP HTTP logs. |
| `--otel-metric-http-json` | none | `true` | Use JSON content type for OTLP HTTP metrics. |
| `--otel-trace-http-json` | none | `true` | Use JSON content type for OTLP HTTP traces. |
| `--otel-service-name` | none | caller default | `service.name` resource attribute. FairMQ device wrappers default this to `--service-name`, or to the executable basename when `--service-name` is unset. |
| `--otel-service-namespace` | none | `nestdaq` | `service.namespace` resource attribute. |
| `--otel-service-instance-id` | none | generated UUID | `service.instance.id` resource attribute. FairMQ device wrappers use `--uuid` when this option is unset; otherwise they generate a UUID. |
| `--otel-fairmq-id` | none | empty | `fairmq.id` resource attribute. |
| `--otel-fairmq-device` | none | empty | `fairmq.device` resource attribute. |
| `--otel-fairmq-session` | none | empty | `fairmq.session` resource attribute. |
| `--otel-fairmq-transport` | none | empty | `fairmq.transport` resource attribute. |

Severity names are `nolog`, `trace`, `debug4`, `debug3`, `debug2`, `debug1`,
`debug`, `detail`, `info`, `state`, `warn`, `warning`, `important`, `alarm`,
`error`, `critical`, and `fatal`.

## Examples

Default operation exports logs to the console exporter and leaves metrics and
traces disabled:

```sh
my-device
```

Send logs, metrics, and traces to an OTLP HTTP collector:

```sh
my-device \
  --otel-log-protocol=otlp-http \
  --otel-metric-protocol=otlp-http \
  --otel-trace-protocol=otlp-http \
  --otel-log-endpoint-http=http://collector:4318/v1/logs \
  --otel-metric-endpoint-http=http://collector:4318/v1/metrics \
  --otel-trace-endpoint-http=http://collector:4318/v1/traces
```

Disable logs explicitly by passing the protocol option without a value:

```sh
my-device --otel-log-protocol
```

Use the C++ thin API from an application that manages telemetry explicitly:

```cpp
auto library = nestdaq::telemetry::TelemetryLibrary{};
if (!library.Load("libnestdaq_otel.so")) {
    std::cerr << library.GetLastError() << '\n';
}

auto options = nestdaq::telemetry::TelemetryOptions{};
options.logProtocol = "console";
options.metricProtocol = "otlp-http";

const auto config = nestdaq::telemetry::MakeConfig(options);
if (!library.InitializeWith(config)) {
    std::cerr << library.GetLastError() << '\n';
}

auto telemetry = nestdaq::telemetry::Telemetry{library};
telemetry.AddCounter("events.total", 1, "1", "Total processed events");
telemetry.RecordHistogram("event.size", 4096, "By", "Input event size");
telemetry.RecordGauge("queue.depth", 12, "{message}", "Latest queue depth");

auto events = telemetry.Counter("events.total", "1", "Total processed events");
events.Add(1, {{"channel", "data"}});

auto queueDepth = telemetry.Gauge("queue.depth", "{message}", "Latest queue depth");
queueDepth.Record(12, {{"channel", "data"}});

auto span = telemetry.StartSpan("process-event");
span.SetAttribute({
    .key = "component",
    .type = NESTDAQ_OTEL_ATTRIBUTE_STRING,
    .string_value = "sampler",
    .int_value = 0,
    .uint_value = 0,
    .double_value = 0.0,
    .bool_value = 0,
});
```

## Collector Compose Setup

For a local OpenTelemetry Collector, OpenSearch, and OpenSearch Dashboards
environment, see [OpenTelemetry Collector Compose Setup](../../share/otel-collector-compose/README.md).

## Troubleshooting

- If `--otel-library` cannot be loaded, check `LD_LIBRARY_PATH`, install rpath,
  or pass an absolute path.
- If the library loads but initialization fails, inspect
  `TelemetryLibrary::GetLastError()`.
- Unsupported protocol names, invalid config size, invalid severity values, and
  empty metric/span names are reported through the plugin last-error string.
- A disabled metric signal makes metric recording a successful no-op. A disabled
  trace signal returns an inactive span.
