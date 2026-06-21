# Telemetry

NestDAQ telemetry is an optional OpenTelemetry integration for FairMQ-based
devices and controller processes. The application executable does not link
OpenTelemetry directly. Instead, NestDAQ loads a single runtime plugin,
`libnestdaq_otel.so`, with `dlopen()` and resolves a small C ABI.

The plugin can export three OpenTelemetry signals:

| Signal  | Default            | Source in NestDAQ                                      |
| ------- | ------------------ | ----------------------------------------------------- |
| Logs    | `console` exporter | FairLogger custom sink; optional spdlog sink          |
| Metrics | disabled           | `nestdaq::telemetry::Telemetry` counter/histogram/gauge API |
| Traces  | disabled           | `nestdaq::telemetry::TelemetrySpan` RAII API          |

`libnestdaq_otel.so` is built and installed only when `opentelemetry-cpp` is
found at CMake configure time.

## Runtime Model

NestDAQ installs process-wide OpenTelemetry providers inside the telemetry
plugin. FairLogger logs are captured by a process-wide custom sink. spdlog logs
are exported only from loggers that explicitly attach the NestDAQ spdlog sink.
Metrics and traces are recorded through the NestDAQ thin wrapper API, which
does not expose OpenTelemetry C++ headers.

The runtime plugin keeps the public C ABI in `OpenTelemetryInitializer.cxx` and
organizes the implementation internally by signal area: logs, metrics, traces,
and shared runtime helpers. Applications should use `TelemetryLibrary`,
`Telemetry`, `Counter`, `Histogram`, `Gauge`, `TelemetrySpan`, and
`GetTelemetry()` instead of depending on those internal implementation files.

Each signal accepts a comma-separated protocol list. Supported protocols are
`console`, `otlp-http`, and `otlp-grpc`; the aliases `http`, `otlp_http`, `grpc`,
and `otlp_grpc` are also accepted by the plugin. An empty protocol disables the
signal.

## Resource Attributes

Logs, metrics, and traces share one OpenTelemetry resource. NestDAQ sets these
resource attributes when values are available:

| Attribute | Value |
| --------- | ----- |
| `service.name` | Configured telemetry service name, or `nestdaq` when unset. |
| `service.version` | `NESTDAQ_VERSION`. |
| `service.namespace` | Configured telemetry service namespace. |
| `service.instance.id` | Configured telemetry service instance id. |
| `host.name` | Host name detected at telemetry option parsing time. |
| `nestdaq.instance.id` | FairMQ device id after it is known. |
| `nestdaq.instance.id.status` | `unresolved` before the FairMQ device id is known, otherwise `resolved`. |
| `fairmq.id` | FairMQ device id. |
| `fairmq.device` | FairMQ device name. |
| `fairmq.session` | FairMQ session. |
| `fairmq.transport` | FairMQ transport. |

Detailed NestDAQ and FairMQ build/git metadata is emitted as structured startup
log bodies, not as resource attributes.

## FairLogger Log Records

The FairLogger custom sink converts each emitted FairLogger message into an
OpenTelemetry LogRecord when the message severity is at or above
`--otel-log-severity`.

| LogRecord field or attribute | Source |
| ---------------------------- | ------ |
| Body | FairLogger message text. |
| Timestamp | FairLogger `metadata.timestamp + metadata.us`. |
| Observed timestamp | Time when the custom sink creates the LogRecord. |
| SeverityNumber | OpenTelemetry severity mapped from FairLogger severity. |
| SeverityText | OpenTelemetry-defined text for the mapped severity. |
| `fairlogger.severity.number` | Original FairLogger severity number. |
| `fairlogger.severity.text` | Original FairLogger severity name. |
| `nestdaq.instance.id` | Per-record instance id set through the telemetry loader after the FairMQ device id is known. |
| `nestdaq.instance.name` | Prefix parsed from an instance id ending in `-<number>`. |
| `nestdaq.instance.index` | Numeric suffix parsed from an instance id ending in `-<number>`. |
| `process.name` | FairLogger process name metadata. |
| `code.file.path` | FairLogger source file metadata. |
| `code.line.number` | FairLogger source line metadata. |
| `code.function.name` | FairLogger function metadata. |
| `thread.id` | Native Linux thread id, or a hashed C++ thread id on other platforms. |

The instrumentation scope uses logger/library name `FairLogger` and library
version `FAIRLOGGER_VERSION`. NestDAQ does not add a custom
`log.severity.text` attribute; `SeverityText` is the standard OpenTelemetry
LogRecord field.

FairMQ throughput log lines are parsed for framework metrics before the log
severity filter is applied. A throughput sample can therefore update framework
metrics even when the original log message is below the exported log severity.
Metrics and traces are initialized only after the FairMQ device id is known so
their resource contains `nestdaq.instance.id`. Logs are initialized at process
startup with `nestdaq.instance.id.status=unresolved`, then reinitialized with
`nestdaq.instance.id.status=resolved` when the id becomes available.

## spdlog Log Records

When NestDAQ is built with both `opentelemetry-cpp` and spdlog available,
`nestdaq/telemetry/SpdlogOpenTelemetrySink.h` is installed. The spdlog
instrumentation is independent from FairLogger instrumentation: NestDAQ does not
change spdlog's default logger, registry, or log level. Applications attach the
returned sink to each spdlog logger that should export OpenTelemetry records.

```cpp
#include <nestdaq/telemetry/SpdlogOpenTelemetrySink.h>

#include <spdlog/spdlog.h>

auto logger = spdlog::logger{
    "sampler",
    {nestdaq::telemetry::CreateSpdlogOpenTelemetrySink()},
};
logger.info("event accepted");
```

The usual spdlog member functions, such as `logger.info(...)` and
`logger.warn(...)`, do not automatically attach source location metadata. Use
the standard spdlog macros when OpenTelemetry records should include file path,
line number, and function name:

```cpp
SPDLOG_LOGGER_INFO(&logger, "accepted event {}", eventId);
SPDLOG_LOGGER_WARN(&logger, "queue depth is {}", depth);
```

For the default spdlog logger, use the corresponding default-logger macros:

```cpp
SPDLOG_INFO("accepted event {}", eventId);
SPDLOG_WARN("queue depth is {}", depth);
```

The spdlog sink records these OpenTelemetry fields and attributes:

| LogRecord field or attribute | Source |
| ---------------------------- | ------ |
| Body | spdlog message payload. |
| Timestamp | spdlog message timestamp. |
| Observed timestamp | Time when the sink creates the LogRecord. |
| SeverityNumber | OpenTelemetry severity mapped from spdlog level. |
| SeverityText | OpenTelemetry-defined text for the mapped severity. |
| `spdlog.logger.name` | spdlog logger name. |
| `spdlog.level` | Original spdlog level text. |
| `code.file.path` | spdlog source file metadata, when present. |
| `code.line.number` | spdlog source line metadata, when present. |
| `code.function.name` | spdlog function metadata, when present. |
| `thread.id` | spdlog thread id metadata. |

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
| `--otel-service-name` | none | caller default | `service.name` resource attribute. FairMQ device wrappers default this to `--service-name`, or to the executable basename when `--service-name` is unset. NestDAQ converts ASCII uppercase letters to lowercase because collector pipelines may use this value in OpenSearch index names. |
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
