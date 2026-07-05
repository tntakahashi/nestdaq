# Telemetry

NestDAQ telemetry is an optional OpenTelemetry integration for FairMQ-based
devices and controller processes. The application executable does not link
OpenTelemetry directly. Instead, NestDAQ loads a single runtime plugin,
`libnestdaq_otel.so`, with `dlopen()` and resolves a small C application binary
interface (ABI).

The plugin can export three OpenTelemetry signals:

| Signal  | Default            | Source in NestDAQ                                      |
| ------- | ------------------ | ----------------------------------------------------- |
| Logs    | `console` exporter | FairLogger custom sink; optional spdlog sink          |
| Metrics | disabled           | `nestdaq::telemetry::Telemetry` counter/histogram/gauge application programming interface (API) |
| Traces  | disabled           | `nestdaq::telemetry::TelemetrySpan` resource acquisition is initialization (RAII) API |

`libnestdaq_otel.so` is built and installed only when `opentelemetry-cpp` is
found at CMake configure time.

## 1. Runtime Model

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
`console`, `otlp-http`, and `otlp-grpc`. OTLP means OpenTelemetry Protocol,
HTTP means Hypertext Transfer Protocol, and gRPC means Google remote procedure
call. The aliases `http`, `otlp_http`, `grpc`, and `otlp_grpc` are also
accepted by the plugin. An empty protocol disables the signal.

## 2. Resource Attributes

Logs, metrics, and traces share one OpenTelemetry resource. NestDAQ sets these
resource attributes when values are available. The `service.*` and `host.*`
keys below are OpenTelemetry semantic convention attributes. `OTel` is used
below as the common abbreviation for OpenTelemetry. The `nestdaq.*`
and `fairmq.*` keys are NestDAQ-specific attributes.

| Attribute | Origin | Value |
| --------- | ------ | ----- |
| `service.name` | OTel semantic convention | Configured telemetry service name, or `nestdaq` when unset. |
| `service.version` | OTel semantic convention | `NESTDAQ_VERSION`. |
| `service.namespace` | OTel semantic convention | Configured telemetry service namespace. |
| `service.instance.id` | OTel semantic convention | Configured telemetry service instance id. |
| `host.name` | OTel semantic convention | Host name detected at telemetry option parsing time. |
| `nestdaq.instance.id` | NestDAQ custom | FairMQ device id after it is known. |
| `nestdaq.instance.id.status` | NestDAQ custom | `unresolved` before the FairMQ device id is known, otherwise `resolved`. |
| `fairmq.id` | NestDAQ/FairMQ custom | FairMQ device id. |
| `fairmq.device` | NestDAQ/FairMQ custom | FairMQ device name. |
| `fairmq.session` | NestDAQ/FairMQ custom | FairMQ session. |
| `fairmq.transport` | NestDAQ/FairMQ custom | FairMQ transport. |

Detailed NestDAQ and FairMQ build/git metadata is emitted as structured startup
log bodies, not as resource attributes. The OpenTelemetry software development
kit (SDK) may add its own SDK resource attributes independently; this table
lists attributes explicitly set by NestDAQ.

## 3. FairLogger Log Records

The FairLogger custom sink converts each emitted FairLogger message into an
OpenTelemetry LogRecord when the FairLogger severity is at or above
`--otel-log-severity`.

| LogRecord field or attribute | Origin | Source |
| ---------------------------- | ------ | ------ |
| Body | OTel LogRecord field | FairLogger message text. |
| Timestamp | OTel LogRecord field | FairLogger `metadata.timestamp + metadata.us`. |
| Observed timestamp | OTel LogRecord field | Time when the custom sink creates the LogRecord. |
| SeverityNumber | OTel LogRecord field | OpenTelemetry severity mapped from FairLogger severity. |
| SeverityText | OTel LogRecord field | OpenTelemetry-defined text for the mapped severity. |
| `code.file.path` | OTel semantic convention | FairLogger source file metadata. |
| `code.line.number` | OTel semantic convention | FairLogger source line metadata. |
| `code.function.name` | OTel semantic convention | FairLogger function metadata. |
| `thread.id` | OTel semantic convention | Native Linux thread id, or a hashed C++ thread id on other platforms. |
| `fairlogger.severity.number` | NestDAQ/FairLogger custom | Original FairLogger severity number. |
| `fairlogger.severity.text` | NestDAQ/FairLogger custom | Original FairLogger severity name. |
| `nestdaq.instance.id` | NestDAQ custom | Per-record instance id set through the telemetry loader after the FairMQ device id is known. |
| `nestdaq.instance.name` | NestDAQ custom | Prefix parsed from an instance id ending in `-<number>`. |
| `nestdaq.instance.index` | NestDAQ custom | Numeric suffix parsed from an instance id ending in `-<number>`. |
| `process.name` | NestDAQ/FairLogger custom | FairLogger process name metadata. This is not the OTel `process.executable.name` resource attribute. |

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

## 4. spdlog Log Records

The spdlog OpenTelemetry sink is experimental and not yet fully verified.

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

| LogRecord field or attribute | Origin | Source |
| ---------------------------- | ------ | ------ |
| Body | OTel LogRecord field | spdlog message payload. |
| Timestamp | OTel LogRecord field | spdlog message timestamp. |
| Observed timestamp | OTel LogRecord field | Time when the sink creates the LogRecord. |
| SeverityNumber | OTel LogRecord field | OpenTelemetry severity mapped from spdlog level. |
| SeverityText | OTel LogRecord field | OpenTelemetry-defined text for the mapped severity. |
| `code.file.path` | OTel semantic convention | spdlog source file metadata, when present. |
| `code.line.number` | OTel semantic convention | spdlog source line metadata, when present. |
| `code.function.name` | OTel semantic convention | spdlog function metadata, when present. |
| `thread.id` | OTel semantic convention | spdlog thread id metadata. |
| `spdlog.logger.name` | NestDAQ/spdlog custom | spdlog logger name. |
| `spdlog.level` | NestDAQ/spdlog custom | Original spdlog level text. |

## 5. Log Severity Mapping

OpenTelemetry stores the normalized log level in the LogRecord
`SeverityNumber` and `SeverityText` fields. The original logging-library level
is kept separately as `fairlogger.severity.*` for FairLogger records and
`spdlog.level` for spdlog records.
The logging-library enum integers are not OpenTelemetry `SeverityNumber`
values; use the OpenTelemetry fields for normalized severity queries.

`--otel-log-severity` is a FairLogger sink filter. It controls the minimum
FairLogger severity exported to OpenTelemetry logs. It does not filter records
emitted through the optional spdlog sink; spdlog filtering remains controlled by
the spdlog logger and sink levels.

### 5.1. FairLogger Severity Mapping

| FairLogger level | `fair::Severity` int | OTel SeverityNumber | OTel SeverityText | Original level attributes |
| ---------------- | -------------------- | ------------------- | ----------------- | ------------------------- |
| `nolog` | `0` | `0` | invalid / unspecified | `fairlogger.severity.*` |
| `trace` | `1` | `1` | `TRACE` | `fairlogger.severity.*` |
| `debug4` | `2` | `2` | `TRACE2` | `fairlogger.severity.*` |
| `debug3` | `3` | `2` | `TRACE2` | `fairlogger.severity.*` |
| `debug2` | `4` | `3` | `TRACE3` | `fairlogger.severity.*` |
| `debug1` | `5` | `4` | `TRACE4` | `fairlogger.severity.*` |
| `debug` | `6` | `5` | `DEBUG` | `fairlogger.severity.*` |
| `detail` | `7` | `6` | `DEBUG2` | `fairlogger.severity.*` |
| `info` | `8` | `9` | `INFO` | `fairlogger.severity.*` |
| `state` | `9` | `10` | `INFO2` | `fairlogger.severity.*` |
| `warn` | `10` | `13` | `WARN` | `fairlogger.severity.*` |
| `important` | `11` | `14` | `WARN2` | `fairlogger.severity.*` |
| `alarm` | `12` | `15` | `WARN3` | `fairlogger.severity.*` |
| `error` | `13` | `17` | `ERROR` | `fairlogger.severity.*` |
| `critical` | `14` | `18` | `ERROR2` | `fairlogger.severity.*` |
| `fatal` | `15` | `21` | `FATAL` | `fairlogger.severity.*` |

`warning` is accepted as a `--otel-log-severity` alias for `warn`; FairLogger
records themselves use the FairLogger level names. The alias has the same
`fair::Severity` value as `warn`, `10`.

### 5.2. spdlog Severity Mapping

| spdlog level | `spdlog::level::level_enum` int | OTel SeverityNumber | OTel SeverityText | Original level attribute |
| ------------ | -------------------------------- | ------------------- | ----------------- | ------------------------ |
| `trace` | `0` | `1` | `TRACE` | `spdlog.level` |
| `debug` | `1` | `5` | `DEBUG` | `spdlog.level` |
| `info` | `2` | `9` | `INFO` | `spdlog.level` |
| `warn` | `3` | `13` | `WARN` | `spdlog.level` |
| `err` | `4` | `17` | `ERROR` | `spdlog.level` |
| `critical` | `5` | `21` | `FATAL` | `spdlog.level` |
| `off` | `6` | `0` | invalid / unspecified | `spdlog.level` |
| `n_levels` | `7` | `0` | invalid / unspecified | `spdlog.level` |

## 6. Command-Line Options

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
| `--spdlog-console-pattern` | `NESTDAQ_SPDLOG_CONSOLE_PATTERN` | `[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v` | spdlog native console sink pattern. |
| `--spdlog-native-console` | `NESTDAQ_SPDLOG_NATIVE_CONSOLE` | `true` | Enable spdlog native console output independently from the OTel spdlog sink. |
| `--spdlog-async` | `NESTDAQ_SPDLOG_ASYNC` | `false` | Use `spdlog::async_logger` for NestDAQ helper loggers. |
| `--spdlog-async-queue-size` | `NESTDAQ_SPDLOG_ASYNC_QUEUE_SIZE` | `8192` | Queue size for async spdlog helper loggers. |
| `--spdlog-async-thread-count` | `NESTDAQ_SPDLOG_ASYNC_THREAD_COUNT` | `1` | Worker thread count for async spdlog helper loggers. |
| `--spdlog-async-overflow-policy` | `NESTDAQ_SPDLOG_ASYNC_OVERFLOW_POLICY` | `block` | Queue overflow policy: `block`, `overrun_oldest`, or `discard_new`. |
| `--otel-metric-export-interval-ms` | none | `1000` | Periodic metric export interval in milliseconds. |
| `--otel-log-http-json` | none | `true` | Use JavaScript Object Notation (JSON) content type for OTLP HTTP logs. |
| `--otel-metric-http-json` | none | `true` | Use JSON content type for OTLP HTTP metrics. |
| `--otel-trace-http-json` | none | `true` | Use JSON content type for OTLP HTTP traces. |
| `--otel-service-name` | none | caller default | `service.name` resource attribute. FairMQ device wrappers default this to `--service-name`, or to the executable basename when `--service-name` is unset. NestDAQ converts ASCII uppercase letters to lowercase because collector pipelines may use this value in OpenSearch index names. |
| `--otel-service-namespace` | none | `nestdaq` | `service.namespace` resource attribute. |
| `--otel-service-instance-id` | none | generated universally unique identifier (UUID) | `service.instance.id` resource attribute. FairMQ device wrappers use `--uuid` when this option is unset; otherwise they generate a UUID. |
| `--otel-fairmq-id` | none | empty | `fairmq.id` resource attribute. |
| `--otel-fairmq-device` | none | empty | `fairmq.device` resource attribute. |
| `--otel-fairmq-session` | none | empty | `fairmq.session` resource attribute. |
| `--otel-fairmq-transport` | none | empty | `fairmq.transport` resource attribute. |

Severity names are `nolog`, `trace`, `debug4`, `debug3`, `debug2`, `debug1`,
`debug`, `detail`, `info`, `state`, `warn`, `warning`, `important`, `alarm`,
`error`, `critical`, and `fatal`.

## 7. Examples

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

Use spdlog's native console sink with a custom pattern by setting the spdlog
pattern. The native console sink is enabled by default and can run alongside
the OTel spdlog sink:

```sh
my-device \
  --otel-log-protocol=otlp-grpc \
  --spdlog-console-pattern '[%n] [%l] %v'
```

Disable only the native spdlog console output while keeping OTel spdlog export
enabled:

```sh
my-device \
  --otel-log-protocol=otlp-grpc \
  --spdlog-native-console=false
```

NestDAQ helper loggers are synchronous by default and use spdlog multi-thread
safe sinks. Enable async mode when logging frequency is high enough that caller
threads should hand records to a background worker:

```sh
my-device \
  --spdlog-async=true \
  --spdlog-async-queue-size=16384 \
  --spdlog-async-thread-count=2 \
  --spdlog-async-overflow-policy=block
```

The `block` overflow policy avoids losing log records but can make caller
threads wait when the queue is full. `overrun_oldest` drops old queued records,
and `discard_new` drops newly submitted records when the queue is full. Async
queue size and worker count are used when async helper loggers are created;
changing them later does not modify existing loggers.

Use the C++ thin API from an application that manages telemetry explicitly:

```cpp
auto library = nestdaq::telemetry::TelemetryLibrary{};
if (!library.Load("libnestdaq_otel.so")) {
    std::cerr << library.GetLastError() << '\n';
}

auto options = nestdaq::telemetry::TelemetryOptions{};
options.log_protocol = "console";
options.metric_protocol = "otlp-http";

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

The recommended application-facing form is the `Attribute` wrapper used by
`events.Add(...)`, `queueDepth.Record(...)`, and `StartSpan(..., { ... })`. It
keeps string storage alive while NestDAQ converts attributes to the C ABI form
and is the form examples should normally use.

Advanced code can pass prebuilt C ABI attributes directly. This avoids the
temporary `Attribute` wrapper conversion and is useful for hot paths or code
that already owns a `nestdaq_otel_attribute` buffer:

```cpp
std::array<nestdaq_otel_attribute, 2> attributes{{
    {
        .key = "channel",
        .type = NESTDAQ_OTEL_ATTRIBUTE_STRING,
        .string_value = "data",
        .int_value = 0,
        .uint_value = 0,
        .double_value = 0.0,
        .bool_value = 0,
    },
    {
        .key = "slot",
        .type = NESTDAQ_OTEL_ATTRIBUTE_UINT64,
        .string_value = "",
        .int_value = 0,
        .uint_value = 2,
        .double_value = 0.0,
        .bool_value = 0,
    },
}};

telemetry.AddCounter(
    "events.total", 1, "1", "Total processed events", attributes.data(), attributes.size());
```

The caller owns the low-level attribute array and its string storage. NestDAQ
only reads it during the telemetry call. In C++20 builds, equivalent overloads
also accept `std::span<const nestdaq_otel_attribute>` and forward to the same
low-level implementation.

## 8. Collector Compose Setup

For a local OpenTelemetry Collector, OpenSearch, and OpenSearch Dashboards
environment, see [OpenTelemetry Collector Compose Setup](../../share/otel-collector-compose/README.md).

## 9. Troubleshooting

- If `--otel-library` cannot be loaded, check `LD_LIBRARY_PATH`, install rpath,
  or pass an absolute path.
- If the library loads but initialization fails, inspect
  `TelemetryLibrary::GetLastError()`.
- Unsupported protocol names, invalid config size, invalid severity values, and
  empty metric/span names are reported through the plugin last-error string.
- A disabled metric signal makes metric recording a successful no-op. A disabled
  trace signal returns an inactive span.
