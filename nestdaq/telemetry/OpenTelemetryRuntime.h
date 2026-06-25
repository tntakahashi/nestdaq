/** @file
 *  @brief Internal OpenTelemetry plugin runtime state and helper declarations.
 *
 * This header is shared only by the `libnestdaq_otel.so` implementation files.
 * It may include OpenTelemetry C++ SDK headers because it is not used by
 * NestDAQ executables that should remain OpenTelemetry-unlinked.
 */

#pragma once

#include "nestdaq/telemetry/OpenTelemetryInitializer.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <opentelemetry/common/attribute_value.h>
#include <opentelemetry/exporters/otlp/otlp_environment.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/observer_result.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/processor.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/metrics/metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>

namespace nestdaq::telemetry {
struct FairMQThroughputSample;
}

namespace nestdaq::otel_detail {

/** @brief Export protocol selected for one OpenTelemetry signal. */
enum class Protocol : std::uint8_t {
    Console,
    OtlpHttp,
    OtlpGrpc,
};

/** @brief User metric instrument kind used as part of instrument identity. */
enum class MetricKind : std::uint8_t {
    DoubleCounter,
    DoubleHistogram,
    DoubleGauge,
};

inline constexpr std::string_view kDefaultLogProtocol{"console"};
inline constexpr std::string_view kDefaultLogHttpEndpoint{"http://localhost:4318/v1/logs"};
inline constexpr std::string_view kDefaultMetricHttpEndpoint{"http://localhost:4318/v1/metrics"};
inline constexpr std::string_view kDefaultTraceHttpEndpoint{"http://localhost:4318/v1/traces"};
inline constexpr std::string_view kDefaultGrpcEndpoint{"localhost:4317"};
inline constexpr uint32_t kDefaultMetricExportIntervalMs{1000};

/**
 * @brief Owns strings backing an OpenTelemetry key/value iterable view.
 */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct AttributeStorage {
    std::vector<std::string> keys;
    std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>> values;
};

/**
 * @brief Stable key for lazily created user metric instruments.
 */
struct MetricKey {
    MetricKind kind{MetricKind::DoubleCounter};
    std::string name;
    std::string unit;
    std::string description;

    auto operator<(const MetricKey &other) const -> bool
    {
        return std::tie(kind, name, unit, description) <
               std::tie(other.kind, other.name, other.unit, other.description);
    }

    auto operator==(const MetricKey &other) const -> bool
    {
        return std::tie(kind, name, unit, description) ==
               std::tie(other.kind, other.name, other.unit, other.description);
    }
};

/**
 * @brief Comparable representation of one gauge attribute.
 *
 * Observable gauge samples are keyed by full attribute set, so attributes need
 * value semantics independent of the borrowed C ABI pointers passed by callers.
 */
struct GaugeAttribute {
    std::string key;
    nestdaq_otel_attribute_type type{NESTDAQ_OTEL_ATTRIBUTE_STRING};
    std::string stringValue;
    int64_t intValue{0};
    uint64_t uintValue{0};
    double doubleValue{0.0};
    bool boolValue{false};

    auto operator<(const GaugeAttribute &other) const -> bool
    {
        return std::tie(key, type, stringValue, intValue, uintValue, doubleValue, boolValue) <
               std::tie(other.key,
                        other.type,
                        other.stringValue,
                        other.intValue,
                        other.uintValue,
                        other.doubleValue,
                        other.boolValue);
    }

    auto operator==(const GaugeAttribute &other) const -> bool
    {
        return std::tie(key, type, stringValue, intValue, uintValue, doubleValue, boolValue) ==
               std::tie(other.key,
                        other.type,
                        other.stringValue,
                        other.intValue,
                        other.uintValue,
                        other.doubleValue,
                        other.boolValue);
    }
};

/**
 * @brief Map key for the latest user gauge value for one instrument/attribute set.
 */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct GaugeSampleKey {
    MetricKey metric;
    std::vector<GaugeAttribute> attributes;

    auto operator<(const GaugeSampleKey &other) const -> bool
    {
        return std::tie(metric, attributes) < std::tie(other.metric, other.attributes);
    }
};

/** @brief Stored latest user gauge value and its owned attributes. */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct GaugeMeasurement {
    std::vector<GaugeAttribute> attributes;
    double value{0.0};
};

/**
 * @brief Observable gauge plus the key used by its callback to find samples.
 */
struct GaugeInstrument {
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> instrument;
    MetricKey callbackKey;
};

/**
 * @brief One pending FairMQ throughput sample for framework metrics export.
 */
struct FairMQThroughputMeasurement {
    std::string channelName;
    std::string subChannelName;
    std::string direction;
    std::optional<uint64_t> subChannelIndex;
    double messagesPerSecond = 0.0;
    double megabytesPerSecond = 0.0;
};

/** @brief One pending process metrics sample for framework metrics export. */
struct ProcessUsageMeasurement {
    double cpuUserSeconds = 0.0;
    double cpuSystemSeconds = 0.0;
    std::optional<double> cpuUtilization;
    double memoryUsageBytes = 0.0;
};

/** @brief Previous process CPU sample used to compute usage deltas. */
struct ProcessCpuUsageSample {
    std::chrono::steady_clock::time_point timestamp;
    double userSeconds = 0.0;
    double systemSeconds = 0.0;
};

/** @brief One pending FairMQ state transition for framework metrics export. */
struct FairMQStateMeasurement {
    int64_t stateId = 0;
    std::string stateName;
};

/**
 * @brief Owned copy of one signal's C ABI configuration.
 */
struct SignalConfigStorage {
    std::string protocol;
    std::string endpointHttp;
    std::string endpointGrpc;
    std::string headers;
    uint32_t otlpHttpJson = 1U;
};

/**
 * @brief Owned framework metrics configuration used when recreating readers.
 */
struct FrameworkMetricConfigStorage {
    SignalConfigStorage metrics;
    uint32_t timeoutMs = 5000;
    uint32_t metricExportIntervalMs = kDefaultMetricExportIntervalMs;

    auto ToConfig() const -> nestdaq_otel_config
    {
        auto config = nestdaq_otel_config{};
        config.size = sizeof(config);
        config.metrics.protocol = metrics.protocol.data();
        config.metrics.endpoint_http = metrics.endpointHttp.data();
        config.metrics.endpoint_grpc = metrics.endpointGrpc.data();
        config.metrics.headers = metrics.headers.data();
        config.metrics.otlp_http_json = metrics.otlpHttpJson;
        config.timeout_ms = timeoutMs;
        config.metric_export_interval_ms = metricExportIntervalMs;
        return config;
    }
};

/**
 * @brief Process-wide plugin state protected by @ref RuntimeState::mutex.
 *
 * User metrics and framework metrics are intentionally separated: user metrics
 * are held by the normal meter provider, while framework metrics use pending
 * buffers and a dedicated framework meter provider so stale samples are not
 * re-exported after a flush.
 */
struct RuntimeState {
    std::recursive_mutex mutex;
    std::mutex frameworkReconfigureMutex;
    std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
    std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
    std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> frameworkMeterProvider;
    std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> frameworkMeter;
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer;
    std::map<MetricKey, opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<double>>> doubleCounters;
    std::map<MetricKey, opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>>> doubleHistograms;
    std::map<MetricKey, GaugeInstrument> doubleGauges;
    std::map<GaugeSampleKey, double> doubleGaugeMeasurements;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> fairmqMessagesPerSecondGauge;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> fairmqMegabytesPerSecondGauge;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> processCpuTimeCounter;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> processCpuUtilizationGauge;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> processMemoryUsageCounter;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> fairmqStateGauge;
    std::vector<FairMQThroughputMeasurement> pendingFairMQThroughputMeasurements;
    std::vector<FairMQThroughputMeasurement> exportingFairMQThroughputMeasurements;
    std::vector<ProcessUsageMeasurement> pendingProcessUsageMeasurements;
    std::vector<ProcessUsageMeasurement> exportingProcessUsageMeasurements;
    std::vector<FairMQStateMeasurement> pendingFairMQStateMeasurements;
    std::vector<FairMQStateMeasurement> exportingFairMQStateMeasurements;
    std::optional<ProcessCpuUsageSample> processCpuUsageSample;
    long pageSize = 0;
    double availableCpuCount = 0.0;
    std::thread processMetricsThread;
    std::atomic<bool> stopProcessMetricsThread{false};
    std::chrono::milliseconds processMetricsInterval{kDefaultMetricExportIntervalMs};
    std::vector<Protocol> frameworkMetricProtocols;
    FrameworkMetricConfigStorage frameworkMetricConfig;
    std::optional<opentelemetry::sdk::resource::Resource> frameworkMetricResource;
    std::map<uint64_t, opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>> spans;
    std::atomic<uint64_t> nextSpanHandle{1};
    std::string lastError;
};

/** @brief Add a non-empty string resource attribute. */
auto AddStringAttribute(opentelemetry::sdk::resource::ResourceAttributes &attributes,
                        const char *key,
                        const char *value) -> void;
/** @brief Append one validated C ABI attribute to owned iterable storage. */
auto AppendAttribute(AttributeStorage &storage, const nestdaq_otel_attribute &attribute) -> void;
/** @brief Build owned iterable storage from a C ABI attribute array. */
auto BuildAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> AttributeStorage;
/** @brief Build comparable gauge attributes from a C ABI attribute array. */
auto BuildGaugeAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> std::vector<GaugeAttribute>;
/** @brief Clear the process-wide last-error string. */
auto ClearLastError() -> void;
/** @brief Create the log exporter selected by @p protocol. */
auto CreateLogExporter(const nestdaq_otel_config &config, Protocol protocol)
    -> std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>;
/** @brief Wrap a log exporter in the processor appropriate for @p protocol. */
auto CreateLogProcessor(std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter,
                        Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>;
/** @brief Create the metric exporter selected by @p protocol. */
auto CreateMetricExporter(const nestdaq_otel_config &config, Protocol protocol)
    -> std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>;
/** @brief Create a periodic metric reader for one metric exporter. */
auto CreateMetricReader(std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
                        const nestdaq_otel_config &config)
    -> std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>;
/** @brief Create the span exporter selected by @p protocol. */
auto CreateSpanExporter(const nestdaq_otel_config &config, Protocol protocol)
    -> std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>;
/** @brief Wrap a span exporter in the processor appropriate for @p protocol. */
auto CreateSpanProcessor(std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter,
                         Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>;
/** @brief Register FairMQ throughput observable instruments on the framework meter. */
auto ConfigureFairMQThroughputMetrics(RuntimeState &state) -> void;
/** @brief Register process CPU/RSS observable instruments on the framework meter. */
auto ConfigureProcessMetrics(RuntimeState &state) -> void;
/** @brief Register FairMQ state observable instruments on the framework meter. */
auto ConfigureFairMQStateMetrics(RuntimeState &state) -> void;
/** @brief Recreate the framework metrics provider and its observable instruments. */
auto ConfigureFrameworkMetricsProvider(RuntimeState &state) -> void;
/** @brief Return the plugin defaults used when no C ABI config is supplied. */
auto DefaultConfig() -> nestdaq_otel_config;
/** @brief Export and clear pending framework metric samples only when dirty. */
auto FlushFrameworkMetricsIfDirty(uint64_t timeoutMs) -> int;
/** @brief Build the structured FairMQ metadata log body emitted at initialization. */
auto FairMQMetadataLogBody(const nestdaq_otel_config &config) -> std::string;
/** @brief Install OpenTelemetry no-op providers after shutdown. */
auto InstallNoopProviders() -> void;
/** @brief Return true for null or empty C strings. */
auto IsEmpty(const char *value) noexcept -> bool;
/** @brief Build OpenTelemetry resource attributes from the C ABI config. */
auto MakeResource(const nestdaq_otel_config &config) -> opentelemetry::sdk::resource::Resource;
/** @brief Convert optional metadata C strings to a printable value. */
auto MetadataValue(const char *value) -> std::string;
/** @brief Convert optional metadata string_views to a printable value. */
auto MetadataValue(std::string_view value) -> std::string;
/** @brief Build the structured NestDAQ metadata log body emitted at initialization. */
auto NestDAQMetadataLogBody() -> std::string;
/** @brief Parse comma-separated OTLP headers into exporter options. */
auto ParseHeaders(const char *headers) -> opentelemetry::exporter::otlp::OtlpHeaders;
/** @brief Parse a comma-separated protocol list for one signal. */
auto ParseProtocols(const char *protocols, std::vector<Protocol> &out) -> bool;
/** @brief Store @p message as last error and return `NESTDAQ_OTEL_ERROR`. */
auto SetLastError(std::string message) -> int;
/** @brief Return true when a signal config names at least one protocol. */
auto SignalEnabled(const nestdaq_otel_signal_config &config) noexcept -> bool;
/** @brief Start the background CPU/RSS sampler for framework metrics. */
auto StartProcessMetricsThread(uint32_t intervalMs) -> void;
/** @brief Return the process-wide telemetry plugin state. */
auto State() -> RuntimeState &;
/** @brief Copy framework metric configuration and resource for future reconfiguration. */
auto StoreFrameworkMetricConfig(RuntimeState &state,
                                const nestdaq_otel_config &config,
                                const std::vector<Protocol> &protocols,
                                opentelemetry::sdk::resource::Resource resource) -> void;
/** @brief Stop and join the background CPU/RSS sampler if it is running. */
auto StopProcessMetricsThread() -> void;
/** @brief Convert a millisecond timeout to the SDK duration type. */
auto TimeoutFromMs(uint64_t timeoutMs) noexcept -> std::chrono::microseconds;
/** @brief Validate a C ABI attribute before converting it to SDK storage. */
auto ValidateAttribute(const nestdaq_otel_attribute *attribute) noexcept -> bool;
/** @brief Validate that a numeric severity is in the FairLogger range. */
auto ValidateSeverity(int32_t severity) noexcept -> bool;

} // namespace nestdaq::otel_detail
