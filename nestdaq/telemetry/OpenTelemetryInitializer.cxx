/** @file
 *  @brief Implements dynamic OpenTelemetry setup for logs, metrics, and traces.
 */

#include "nestdaq/telemetry/OpenTelemetryInitializer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <opentelemetry/common/key_value_iterable_view.h>
#include <opentelemetry/context/context.h>
#include <opentelemetry/exporters/ostream/log_record_exporter_factory.h>
#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/logs/noop.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/noop.h>
#include <opentelemetry/metrics/observer_result.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/metrics/sync_instruments.h>
#include <opentelemetry/nostd/variant.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_options.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/provider.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/metrics/view/view_registry_factory.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/noop.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/tracer.h>

#include <fairlogger/Logger.h>
#include <fairmq/Version.h>

#include <sys/resource.h>
#include <unistd.h>

#include "nestdaq/telemetry/FairLoggerOpenTelemetrySink.h"
#include "nestdaq/telemetry/FairMQThroughputLogParser.h"

#if __has_include("nestdaq/version.h")
#  include "nestdaq/version.h"
#else
static constexpr std::string_view NESTDAQ_VERSION {"unknown"};
static constexpr std::string_view NESTDAQ_VERSION_PRERELEASE {"unknown"};
static constexpr std::string_view NESTDAQ_BUILD_TYPE {"unknown"};
static constexpr std::string_view NESTDAQ_GIT_COMMIT_DATE {"unknown"};
static constexpr std::string_view NESTDAQ_GIT_BRANCH {"unknown"};
static constexpr std::string_view NESTDAQ_GIT_REMOTE_URL {"unknown"};
static constexpr uint64_t NESTDAQ_GIT_COMMIT_COUNT = 0;
static constexpr std::string_view NESTDAQ_GIT_COMMIT_HASH_STRING {"unknown"};
static constexpr uint64_t NESTDAQ_VERSION_MAJOR = 0;
static constexpr uint64_t NESTDAQ_VERSION_MINOR = 0;
static constexpr uint64_t NESTDAQ_VERSION_PATCH = 0;
#endif

namespace nestdaq {
namespace {

enum class Protocol : std::uint8_t {
    Console,
    OtlpHttp,
    OtlpGrpc,
};

enum class MetricKind : std::uint8_t {
    DoubleCounter,
    DoubleHistogram,
    DoubleGauge,
};

constexpr std::string_view kDefaultLogProtocol{"console"};
constexpr std::string_view kDefaultLogHttpEndpoint{"http://localhost:4318/v1/logs"};
constexpr std::string_view kDefaultMetricHttpEndpoint{"http://localhost:4318/v1/metrics"};
constexpr std::string_view kDefaultTraceHttpEndpoint{"http://localhost:4318/v1/traces"};
constexpr std::string_view kDefaultGrpcEndpoint{"localhost:4317"};
constexpr uint32_t kDefaultMetricExportIntervalMs{60000};

struct AttributeStorage {
    std::vector<std::string> keys;
    std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>> values;
};

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

struct GaugeSampleKey {
    MetricKey metric;
    std::vector<GaugeAttribute> attributes;

    auto operator<(const GaugeSampleKey &other) const -> bool
    {
        return std::tie(metric, attributes) < std::tie(other.metric, other.attributes);
    }
};

struct GaugeMeasurement {
    std::vector<GaugeAttribute> attributes;
    double value{0.0};
};

struct GaugeInstrument {
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> instrument;
    MetricKey callbackKey;
};

struct FairMQThroughputMeasurement {
    std::string channelName;
    std::string subChannelName;
    std::string direction;
    std::optional<uint64_t> subChannelIndex;
    double messagesPerSecond = 0.0;
    double megabytesPerSecond = 0.0;
};

struct ProcessCpuUsageSample {
    std::chrono::steady_clock::time_point timestamp;
    double cpuSeconds = 0.0;
};

struct RuntimeState {
    std::mutex mutex;
    std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
    std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
    std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> meter;
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> tracer;
    std::map<MetricKey, opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Counter<double>>> doubleCounters;
    std::map<MetricKey, opentelemetry::nostd::unique_ptr<opentelemetry::metrics::Histogram<double>>> doubleHistograms;
    std::map<MetricKey, GaugeInstrument> doubleGauges;
    std::map<GaugeSampleKey, double> doubleGaugeMeasurements;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> fairmqMessagesPerSecondGauge;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> fairmqMegabytesPerSecondGauge;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> processCpuUsageGauge;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> processMemoryRssGauge;
    std::map<std::pair<std::string, std::string>, FairMQThroughputMeasurement> fairmqThroughputMeasurements;
    std::optional<ProcessCpuUsageSample> processCpuUsageSample;
    long pageSize = 0;
    std::map<uint64_t, opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>> spans;
    std::atomic<uint64_t> nextSpanHandle{1};
    std::string lastError;
};

auto AddStringAttribute(opentelemetry::sdk::resource::ResourceAttributes &attributes,
                        const char *key,
                        const char *value) -> void;
auto AppendAttribute(AttributeStorage &storage, const nestdaq_otel_attribute &attribute) -> void;
auto BuildAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> AttributeStorage;
auto BuildGaugeAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> std::vector<GaugeAttribute>;
auto ClearLastError() -> void;
auto ConfigureFairMQThroughputMetrics(RuntimeState &state) -> void;
auto ConfigureProcessMetrics(RuntimeState &state) -> void;
auto CreateLogExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>;
auto CreateLogProcessor(std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter,
                        Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>;
auto CreateMetricExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>;
auto CreateMetricReader(std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
                        const nestdaq_otel_config &config)
-> std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>;
auto CreateSpanExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>;
auto CreateSpanProcessor(std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter,
                         Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>;
auto DefaultConfig() -> nestdaq_otel_config;
auto InstallNoopProviders() -> void;
auto IsEmpty(const char *value) noexcept -> bool;
auto FairMQMetadataLogBody(const nestdaq_otel_config &config) -> std::string;
auto MakeResource(const nestdaq_otel_config &config) -> opentelemetry::sdk::resource::Resource;
auto MetadataValue(const char *value) -> std::string;
auto MetadataValue(std::string_view value) -> std::string;
auto MetricEndpointHttp(const nestdaq_otel_config &config) -> const char *;
auto NestDAQMetadataLogBody() -> std::string;
auto MetricEndpointGrpc(const nestdaq_otel_config &config) -> const char *;
auto LogEndpointHttp(const nestdaq_otel_config &config) -> const char *;
auto LogEndpointGrpc(const nestdaq_otel_config &config) -> const char *;
auto ParseHeaders(const char *headers) -> opentelemetry::exporter::otlp::OtlpHeaders;
auto ParseProtocols(const char *protocols, std::vector<Protocol> &out) -> bool;
auto ParseProtocolToken(std::string_view protocol, Protocol &out) -> bool;
auto ObserveFairMQMegabytesPerSecond(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void;
auto ObserveFairMQMessagesPerSecond(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void;
auto ObserveFairMQThroughput(opentelemetry::metrics::ObserverResult observer, bool observeMegabytes) noexcept -> void;
auto ObserveProcessCpuUsage(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void;
auto ObserveProcessMemoryRss(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void;
auto ObserveUserDoubleGauge(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void;
auto ReadProcessCpuUsage() noexcept -> std::optional<ProcessCpuUsageSample>;
auto ReadProcessMemoryRssMiB(long pageSize) -> std::optional<double>;
auto SetLastError(std::string message) -> int;
auto SignalEnabled(const nestdaq_otel_signal_config &config) noexcept -> bool;
auto State() -> RuntimeState &;
auto TimevalToSeconds(const timeval &value) noexcept -> double;
auto TimeoutFromMs(uint64_t timeoutMs) noexcept -> std::chrono::microseconds;
auto ToLower(std::string_view value) -> std::string;
auto TraceEndpointHttp(const nestdaq_otel_config &config) -> const char *;
auto TraceEndpointGrpc(const nestdaq_otel_config &config) -> const char *;
auto Trim(std::string_view value) -> std::string_view;
auto ValidateAttribute(const nestdaq_otel_attribute *attribute) noexcept -> bool;
auto ValidateSeverity(int32_t severity) noexcept -> bool;

auto AddStringAttribute(opentelemetry::sdk::resource::ResourceAttributes &attributes,
                        const char *key,
                        const char *value) -> void
{
    if (!IsEmpty(value)) {
        attributes.emplace(key, std::string{value});
    }
}

auto AppendAttribute(AttributeStorage &storage, const nestdaq_otel_attribute &attribute) -> void
{
    if (!ValidateAttribute(&attribute)) {
        return;
    }

    storage.keys.emplace_back(attribute.key);
    auto key = opentelemetry::nostd::string_view{storage.keys.back()};
    switch (attribute.type) {
    case NESTDAQ_OTEL_ATTRIBUTE_STRING:
        storage.values.emplace_back(key, IsEmpty(attribute.string_value) ? "" : attribute.string_value);
        break;
    case NESTDAQ_OTEL_ATTRIBUTE_INT64:
        storage.values.emplace_back(key, attribute.int_value);
        break;
    case NESTDAQ_OTEL_ATTRIBUTE_UINT64:
        storage.values.emplace_back(key, attribute.uint_value);
        break;
    case NESTDAQ_OTEL_ATTRIBUTE_DOUBLE:
        storage.values.emplace_back(key, attribute.double_value);
        break;
    case NESTDAQ_OTEL_ATTRIBUTE_BOOL:
        storage.values.emplace_back(key, attribute.bool_value != 0);
        break;
    }
}

auto BuildAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> AttributeStorage
{
    auto storage = AttributeStorage{};
    storage.keys.reserve(attributeCount);
    storage.values.reserve(attributeCount);
    if (attributes == nullptr) {
        return storage;
    }
    for (uint64_t i = 0; i < attributeCount; ++i) {
        AppendAttribute(storage, attributes[i]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return storage;
}

auto BuildGaugeAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> std::vector<GaugeAttribute>
{
    auto values = std::vector<GaugeAttribute>{};
    values.reserve(attributeCount);
    if (attributes == nullptr) {
        return values;
    }

    for (uint64_t i = 0; i < attributeCount; ++i) {
        const auto &attribute = attributes[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (!ValidateAttribute(&attribute)) {
            continue;
        }

        auto value = GaugeAttribute{};
        value.key = attribute.key;
        value.type = attribute.type;
        switch (attribute.type) {
        case NESTDAQ_OTEL_ATTRIBUTE_STRING:
            value.stringValue = IsEmpty(attribute.string_value) ? "" : attribute.string_value;
            break;
        case NESTDAQ_OTEL_ATTRIBUTE_INT64:
            value.intValue = attribute.int_value;
            break;
        case NESTDAQ_OTEL_ATTRIBUTE_UINT64:
            value.uintValue = attribute.uint_value;
            break;
        case NESTDAQ_OTEL_ATTRIBUTE_DOUBLE:
            value.doubleValue = attribute.double_value;
            break;
        case NESTDAQ_OTEL_ATTRIBUTE_BOOL:
            value.boolValue = attribute.bool_value != 0;
            break;
        }
        values.emplace_back(std::move(value));
    }
    std::sort(values.begin(), values.end());
    return values;
}

auto ClearLastError() -> void
{
    auto &state = State();
    std::lock_guard lock{state.mutex};
    state.lastError.clear();
}

auto ConfigureFairMQThroughputMetrics(RuntimeState &state) -> void
{
    if (!state.meter) {
        return;
    }

    state.fairmqMessagesPerSecondGauge = state.meter->CreateDoubleObservableGauge(
        "fairmq.channel.messages_per_second",
        "FairMQ channel message rate parsed from Device throughput logs",
        "{message}/s");
    if (state.fairmqMessagesPerSecondGauge) {
        state.fairmqMessagesPerSecondGauge->AddCallback(ObserveFairMQMessagesPerSecond, nullptr);
    }

    state.fairmqMegabytesPerSecondGauge = state.meter->CreateDoubleObservableGauge(
        "fairmq.channel.megabytes_per_second",
        "FairMQ channel throughput parsed from Device throughput logs",
        "MB/s");
    if (state.fairmqMegabytesPerSecondGauge) {
        state.fairmqMegabytesPerSecondGauge->AddCallback(ObserveFairMQMegabytesPerSecond, nullptr);
    }
}

auto ConfigureProcessMetrics(RuntimeState &state) -> void
{
    if (!state.meter) {
        return;
    }

    state.pageSize = sysconf(_SC_PAGESIZE);
    state.processCpuUsageSample = ReadProcessCpuUsage();

    state.processCpuUsageGauge = state.meter->CreateDoubleObservableGauge(
        "process.cpu.usage_percent",
        "Process CPU usage in top/htop style percent",
        "%");
    if (state.processCpuUsageGauge) {
        state.processCpuUsageGauge->AddCallback(ObserveProcessCpuUsage, nullptr);
    }

    state.processMemoryRssGauge = state.meter->CreateDoubleObservableGauge(
        "process.memory.rss_mib",
        "Process resident memory usage",
        "MiBy");
    if (state.processMemoryRssGauge) {
        state.processMemoryRssGauge->AddCallback(ObserveProcessMemoryRss, nullptr);
    }
}

auto CreateLogExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
{
    switch (protocol) {
    case Protocol::Console:
        return opentelemetry::exporter::logs::OStreamLogRecordExporterFactory::Create();
    case Protocol::OtlpHttp: {
        auto options = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions{};
        options.url = LogEndpointHttp(config);
        options.http_headers = ParseHeaders(config.logs.headers);
        options.content_type = config.logs.otlp_http_json == 0
                               ? opentelemetry::exporter::otlp::HttpRequestContentType::kBinary
                               : opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(options);
    }
    case Protocol::OtlpGrpc: {
        auto options = opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOptions{};
        options.endpoint = LogEndpointGrpc(config);
        options.metadata = ParseHeaders(config.logs.headers);
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterFactory::Create(options);
    }
    }
    return nullptr;
}

auto CreateLogProcessor(std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter,
                        Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
{
    if (protocol == Protocol::Console) {
        return opentelemetry::sdk::logs::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
    }
    auto options = opentelemetry::sdk::logs::BatchLogRecordProcessorOptions{};
    return opentelemetry::sdk::logs::BatchLogRecordProcessorFactory::Create(std::move(exporter), options);
}

auto CreateMetricExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
{
    switch (protocol) {
    case Protocol::Console:
        return opentelemetry::exporter::metrics::OStreamMetricExporterFactory::Create();
    case Protocol::OtlpHttp: {
        auto options = opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions{};
        options.url = MetricEndpointHttp(config);
        options.http_headers = ParseHeaders(config.metrics.headers);
        options.content_type = config.metrics.otlp_http_json == 0
                               ? opentelemetry::exporter::otlp::HttpRequestContentType::kBinary
                               : opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(options);
    }
    case Protocol::OtlpGrpc: {
        auto options = opentelemetry::exporter::otlp::OtlpGrpcMetricExporterOptions{};
        options.endpoint = MetricEndpointGrpc(config);
        options.metadata = ParseHeaders(config.metrics.headers);
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(options);
    }
    }
    return nullptr;
}

auto CreateMetricReader(std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
                        const nestdaq_otel_config &config)
-> std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
{
    auto options = opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions{};
    options.export_interval_millis = std::chrono::milliseconds{
        config.metric_export_interval_ms == 0 ? kDefaultMetricExportIntervalMs : config.metric_export_interval_ms};
    options.export_timeout_millis = std::chrono::milliseconds{config.timeout_ms};
    return opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), options);
}

auto CreateSpanExporter(const nestdaq_otel_config &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
{
    switch (protocol) {
    case Protocol::Console:
        return opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
    case Protocol::OtlpHttp: {
        auto options = opentelemetry::exporter::otlp::OtlpHttpExporterOptions{};
        options.url = TraceEndpointHttp(config);
        options.http_headers = ParseHeaders(config.traces.headers);
        options.content_type = config.traces.otlp_http_json == 0
                               ? opentelemetry::exporter::otlp::HttpRequestContentType::kBinary
                               : opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(options);
    }
    case Protocol::OtlpGrpc: {
        auto options = opentelemetry::exporter::otlp::OtlpGrpcExporterOptions{};
        options.endpoint = TraceEndpointGrpc(config);
        options.metadata = ParseHeaders(config.traces.headers);
        options.timeout = TimeoutFromMs(config.timeout_ms);
        return opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(options);
    }
    }
    return nullptr;
}

auto CreateSpanProcessor(std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter,
                         Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
{
    if (protocol == Protocol::Console) {
        return opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    }
    auto options = opentelemetry::sdk::trace::BatchSpanProcessorOptions{};
    return opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(std::move(exporter), options);
}

auto DefaultConfig() -> nestdaq_otel_config
{
    auto config = nestdaq_otel_config{};
    config.size = sizeof(config);
    config.logs.protocol = kDefaultLogProtocol.data();
    config.logs.endpoint_http = kDefaultLogHttpEndpoint.data();
    config.logs.endpoint_grpc = kDefaultGrpcEndpoint.data();
    config.logs.otlp_http_json = 1U;
    config.metrics.endpoint_http = kDefaultMetricHttpEndpoint.data();
    config.metrics.endpoint_grpc = kDefaultGrpcEndpoint.data();
    config.metrics.otlp_http_json = 1U;
    config.traces.endpoint_http = kDefaultTraceHttpEndpoint.data();
    config.traces.endpoint_grpc = kDefaultGrpcEndpoint.data();
    config.traces.otlp_http_json = 1U;
    config.service_name = "nestdaq";
    config.min_severity = static_cast<int32_t>(fair::Severity::trace);
    config.timeout_ms = 5000;
    config.metric_export_interval_ms = kDefaultMetricExportIntervalMs;
    return config;
}

auto InstallNoopProviders() -> void
{
    opentelemetry::logs::Provider::SetLoggerProvider(
        opentelemetry::nostd::shared_ptr<opentelemetry::logs::LoggerProvider> {
            new opentelemetry::logs::NoopLoggerProvider});
    opentelemetry::metrics::Provider::SetMeterProvider(
        opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> {
            new opentelemetry::metrics::NoopMeterProvider});
    opentelemetry::trace::Provider::SetTracerProvider(
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider> {
            new opentelemetry::trace::NoopTracerProvider});
}

auto IsEmpty(const char *value) noexcept -> bool
{
    return value == nullptr || *value == '\0';
}

auto FairMQMetadataLogBody(const nestdaq_otel_config &config) -> std::string
{
    const auto body = nlohmann::json{
        {"fairmq", {
            {"version", {
                {"string", MetadataValue(FAIRMQ_VERSION)},
                {"major", FAIRMQ_VERSION_MAJOR},
                {"minor", FAIRMQ_VERSION_MINOR},
                {"patch", FAIRMQ_VERSION_PATCH},
                {"git", MetadataValue(config.fairmq_git_version)},
            }},
            {"build", {
                {"type", MetadataValue(config.fairmq_build_type)},
            }},
            {"source", {
                {"repo_url", MetadataValue(config.fairmq_repo_url)},
            }},
            {"license", MetadataValue(config.fairmq_license)},
            {"copyright", MetadataValue(config.fairmq_copyright)},
        }},
    };
    return body.dump();
}

auto MetadataValue(const char *value) -> std::string
{
    return IsEmpty(value) ? std::string{"unknown"} : std::string{value};
}

auto MetadataValue(std::string_view value) -> std::string
{
    return value.empty() ? std::string{"unknown"} : std::string{value};
}

auto NestDAQMetadataLogBody() -> std::string
{
    const auto body = nlohmann::json{
        {"nestdaq", {
            {"version", {
                {"string", MetadataValue(NESTDAQ_VERSION)},
                {"major", NESTDAQ_VERSION_MAJOR},
                {"minor", NESTDAQ_VERSION_MINOR},
                {"patch", NESTDAQ_VERSION_PATCH},
                {"prerelease", std::string{NESTDAQ_VERSION_PRERELEASE}},
            }},
            {"build", {
                {"type", MetadataValue(NESTDAQ_BUILD_TYPE)},
            }},
            {"git", {
                {"commit_count", NESTDAQ_GIT_COMMIT_COUNT},
                {"commit_hash", MetadataValue(NESTDAQ_GIT_COMMIT_HASH_STRING)},
                {"branch", MetadataValue(NESTDAQ_GIT_BRANCH)},
                {"remote_url", MetadataValue(NESTDAQ_GIT_REMOTE_URL)},
                {"commit_date", MetadataValue(NESTDAQ_GIT_COMMIT_DATE)},
            }},
        }},
    };
    return body.dump();
}

auto LogEndpointGrpc(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.logs.endpoint_grpc) ? kDefaultGrpcEndpoint.data() : config.logs.endpoint_grpc;
}

auto LogEndpointHttp(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.logs.endpoint_http) ? kDefaultLogHttpEndpoint.data() : config.logs.endpoint_http;
}

auto MakeResource(const nestdaq_otel_config &config) -> opentelemetry::sdk::resource::Resource
{
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{};
    attributes.emplace("service.name", std::string{IsEmpty(config.service_name) ? "nestdaq" : config.service_name});
    attributes.emplace("service.version", std::string{NESTDAQ_VERSION});
    AddStringAttribute(attributes, "service.namespace", config.service_namespace);
    AddStringAttribute(attributes, "service.instance.id", config.service_instance_id);
    AddStringAttribute(attributes, "fairmq.id", config.fairmq_id);
    AddStringAttribute(attributes, "fairmq.device", config.fairmq_device);
    AddStringAttribute(attributes, "fairmq.session", config.fairmq_session);
    AddStringAttribute(attributes, "fairmq.transport", config.fairmq_transport);
    return opentelemetry::sdk::resource::Resource::Create(attributes);
}

auto MetricEndpointGrpc(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.metrics.endpoint_grpc) ? kDefaultGrpcEndpoint.data() : config.metrics.endpoint_grpc;
}

auto MetricEndpointHttp(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.metrics.endpoint_http) ? kDefaultMetricHttpEndpoint.data() : config.metrics.endpoint_http;
}

auto ParseHeaders(const char *headers) -> opentelemetry::exporter::otlp::OtlpHeaders
{
    auto parsed = opentelemetry::exporter::otlp::OtlpHeaders{};
    if (IsEmpty(headers)) {
        return parsed;
    }
    auto input = std::string_view{headers};
    while (!input.empty()) {
        const auto comma = input.find(',');
        auto item = input.substr(0, comma);
        input = comma == std::string_view::npos ? std::string_view{} : input.substr(comma + 1);
        const auto equals = item.find('=');
        if (equals == std::string_view::npos || equals == 0) {
            continue;
        }
        auto key = Trim(item.substr(0, equals));
        auto value = Trim(item.substr(equals + 1));
        if (!key.empty()) {
            parsed.emplace(std::string{key}, std::string{value});
        }
    }
    return parsed;
}

auto ParseProtocols(const char *protocols, std::vector<Protocol> &out) -> bool
{
    if (protocols == nullptr) {
        return true;
    }
    auto input = std::string_view{protocols};
    while (!input.empty()) {
        const auto comma = input.find(',');
        auto token = Trim(input.substr(0, comma));
        input = comma == std::string_view::npos ? std::string_view{} : input.substr(comma + 1);
        if (token.empty()) {
            continue;
        }
        auto protocol = Protocol::Console;
        if (!ParseProtocolToken(token, protocol)) {
            return false;
        }
        out.emplace_back(protocol);
    }
    return true;
}

auto ParseProtocolToken(std::string_view protocol, Protocol &out) -> bool
{
    const auto normalized = ToLower(protocol);
    if (normalized == "console") {
        out = Protocol::Console;
        return true;
    }
    if (normalized == "otlp-http" || normalized == "http" || normalized == "otlp_http") {
        out = Protocol::OtlpHttp;
        return true;
    }
    if (normalized == "otlp-grpc" || normalized == "grpc" || normalized == "otlp_grpc") {
        out = Protocol::OtlpGrpc;
        return true;
    }
    return false;
}

auto ObserveFairMQMegabytesPerSecond(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void
{
    ObserveFairMQThroughput(observer, true);
}

auto ObserveFairMQMessagesPerSecond(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept
-> void
{
    ObserveFairMQThroughput(observer, false);
}

auto ObserveFairMQThroughput(opentelemetry::metrics::ObserverResult observer, bool observeMegabytes) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto measurements = std::vector<FairMQThroughputMeasurement>{};
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        measurements.reserve(state.fairmqThroughputMeasurements.size());
        for (const auto &[_, measurement] : state.fairmqThroughputMeasurements) {
            measurements.emplace_back(measurement);
        }
    }

    for (const auto &measurement : measurements) {
        auto attributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{};
        attributes.reserve(3);
        attributes.emplace_back("fairmq.channel.name",
                                opentelemetry::nostd::string_view{measurement.channelName});
        attributes.emplace_back("network.io.direction",
                                opentelemetry::nostd::string_view{measurement.direction});
        if (measurement.subChannelIndex) {
            attributes.emplace_back("fairmq.channel.index",
                                    static_cast<int64_t>(*measurement.subChannelIndex));
        }
        result->Observe(observeMegabytes ? measurement.megabytesPerSecond : measurement.messagesPerSecond,
                        attributes);
    }
}

auto ObserveProcessCpuUsage(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    const auto currentSample = ReadProcessCpuUsage();
    if (!currentSample) {
        return;
    }

    auto value = 0.0;
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        if (state.processCpuUsageSample) {
            const auto elapsedSeconds =
                std::chrono::duration<double>{currentSample->timestamp - state.processCpuUsageSample->timestamp}.count();
            if (elapsedSeconds > 0.0) {
                value = ((currentSample->cpuSeconds - state.processCpuUsageSample->cpuSeconds) / elapsedSeconds) * 100.0;
            }
        }
        state.processCpuUsageSample = currentSample;
    }

    result->Observe(value);
}

auto ObserveProcessMemoryRss(opentelemetry::metrics::ObserverResult observer, void * /* state */) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer)) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    auto pageSize = 0L;
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        pageSize = state.pageSize;
    }

    const auto value = ReadProcessMemoryRssMiB(pageSize);
    if (!value) {
        return;
    }
    result->Observe(*value);
}

auto ObserveUserDoubleGauge(opentelemetry::metrics::ObserverResult observer, void *state) noexcept -> void
{
    using DoubleObserver = opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>;
    if (!opentelemetry::nostd::holds_alternative<DoubleObserver>(observer) || state == nullptr) {
        return;
    }

    const auto result = opentelemetry::nostd::get<DoubleObserver>(observer);
    if (!result) {
        return;
    }

    const auto *metric = static_cast<const MetricKey*>(state);
    auto measurements = std::vector<GaugeMeasurement>{};
    {
        auto &runtime = State();
        std::lock_guard lock{runtime.mutex};
        for (const auto &[sample, value] : runtime.doubleGaugeMeasurements) {
            if (sample.metric == *metric) {
                measurements.emplace_back(GaugeMeasurement{
                    .attributes = sample.attributes,
                    .value = value,
                });
            }
        }
    }

    for (const auto &measurement : measurements) {
        auto attributes =
            std::vector<std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{};
        attributes.reserve(measurement.attributes.size());
        for (const auto &attribute : measurement.attributes) {
            auto key = opentelemetry::nostd::string_view{attribute.key};
            switch (attribute.type) {
            case NESTDAQ_OTEL_ATTRIBUTE_STRING:
                attributes.emplace_back(key, opentelemetry::nostd::string_view{attribute.stringValue});
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_INT64:
                attributes.emplace_back(key, attribute.intValue);
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_UINT64:
                attributes.emplace_back(key, attribute.uintValue);
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_DOUBLE:
                attributes.emplace_back(key, attribute.doubleValue);
                break;
            case NESTDAQ_OTEL_ATTRIBUTE_BOOL:
                attributes.emplace_back(key, attribute.boolValue);
                break;
            }
        }
        result->Observe(measurement.value, attributes);
    }
}

auto ReadProcessCpuUsage() noexcept -> std::optional<ProcessCpuUsageSample>
{
    auto usage = rusage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return std::nullopt;
    }
    return ProcessCpuUsageSample{
        .timestamp = std::chrono::steady_clock::now(),
        .cpuSeconds = TimevalToSeconds(usage.ru_utime) + TimevalToSeconds(usage.ru_stime),
    };
}

auto ReadProcessMemoryRssMiB(long pageSize) -> std::optional<double>
{
    if (pageSize <= 0) {
        return std::nullopt;
    }

    auto statm = std::ifstream{"/proc/self/statm"};
    auto totalPages = uint64_t{0};
    auto residentPages = uint64_t{0};
    if (!(statm >> totalPages >> residentPages)) {
        return std::nullopt;
    }

    const auto bytes = static_cast<double>(residentPages) * static_cast<double>(pageSize);
    return bytes / (1024.0 * 1024.0);
}

auto SetLastError(std::string message) -> int
{
    auto &state = State();
    std::lock_guard lock{state.mutex};
    state.lastError = std::move(message);
    return NESTDAQ_OTEL_ERROR;
}

auto SignalEnabled(const nestdaq_otel_signal_config &config) noexcept -> bool
{
    return !IsEmpty(config.protocol);
}

auto State() -> RuntimeState &
{
    static auto state = RuntimeState{};
    return state;
}

auto TimevalToSeconds(const timeval &value) noexcept -> double
{
    return static_cast<double>(value.tv_sec) + (static_cast<double>(value.tv_usec) / 1'000'000.0);
}

auto TimeoutFromMs(uint64_t timeoutMs) noexcept -> std::chrono::microseconds
{
    if (timeoutMs == 0) {
        return (std::chrono::microseconds::max)();
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds{timeoutMs});
}

auto ToLower(std::string_view value) -> std::string
{
    auto out = std::string{value};
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

auto TraceEndpointGrpc(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.traces.endpoint_grpc) ? kDefaultGrpcEndpoint.data() : config.traces.endpoint_grpc;
}

auto TraceEndpointHttp(const nestdaq_otel_config &config) -> const char *
{
    return IsEmpty(config.traces.endpoint_http) ? kDefaultTraceHttpEndpoint.data() : config.traces.endpoint_http;
}

auto Trim(std::string_view value) -> std::string_view
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

auto ValidateAttribute(const nestdaq_otel_attribute *attribute) noexcept -> bool
{
    return attribute != nullptr && !IsEmpty(attribute->key);
}

auto ValidateSeverity(int32_t severity) noexcept -> bool
{
    return severity >= static_cast<int32_t>(fair::Severity::nolog) &&
           static_cast<size_t>(severity) < fair::Logger::fSeverityNames.size();
}

} // namespace

auto OpenTelemetryInitializer::ForceFlush(uint64_t timeout_ms) -> int
{
    try {
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
        std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
        std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;
        {
            auto &state = State();
            std::lock_guard lock{state.mutex};
            loggerProvider = state.loggerProvider;
            meterProvider = state.meterProvider;
            tracerProvider = state.tracerProvider;
        }
        auto ok = true;
        if (loggerProvider) {
            ok = loggerProvider->ForceFlush(TimeoutFromMs(timeout_ms)) && ok;
        }
        if (meterProvider) {
            ok = meterProvider->ForceFlush(TimeoutFromMs(timeout_ms)) && ok;
        }
        if (tracerProvider) {
            ok = tracerProvider->ForceFlush(TimeoutFromMs(timeout_ms)) && ok;
        }
        if (!ok) {
            return SetLastError("OpenTelemetry force flush failed");
        }
        ClearLastError();
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry force flush error");
    }
}

auto OpenTelemetryInitializer::Initialize(const nestdaq_otel_config *config) -> int
{
    auto localConfig = DefaultConfig();
    if (config != nullptr) {
        if (config->size != sizeof(nestdaq_otel_config)) {
            return SetLastError("nestdaq_otel_config has an unsupported size");
        }
        localConfig = *config;
    }
    if (!ValidateSeverity(localConfig.min_severity)) {
        return SetLastError("min_severity must be a valid fair::Severity numeric value");
    }

    auto logProtocols = std::vector<Protocol>{};
    auto metricProtocols = std::vector<Protocol>{};
    auto traceProtocols = std::vector<Protocol>{};
    if ((SignalEnabled(localConfig.logs) && !ParseProtocols(localConfig.logs.protocol, logProtocols)) ||
        (SignalEnabled(localConfig.metrics) && !ParseProtocols(localConfig.metrics.protocol, metricProtocols)) ||
        (SignalEnabled(localConfig.traces) && !ParseProtocols(localConfig.traces.protocol, traceProtocols))) {
        return SetLastError("unsupported OpenTelemetry protocol; expected comma-separated console, otlp-http, or otlp-grpc");
    }

    try {
        auto resource = MakeResource(localConfig);
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
        std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
        std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;

        if (!logProtocols.empty()) {
            auto processors = std::vector<std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>>{};
            for (const auto protocol : logProtocols) {
                processors.emplace_back(CreateLogProcessor(CreateLogExporter(localConfig, protocol), protocol));
            }
            loggerProvider = std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>{
                opentelemetry::sdk::logs::LoggerProviderFactory::Create(std::move(processors), resource)};
        }

        if (!metricProtocols.empty()) {
            auto views = opentelemetry::sdk::metrics::ViewRegistryFactory::Create();
            meterProvider = std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider>{
                opentelemetry::sdk::metrics::MeterProviderFactory::Create(std::move(views), resource)};
            for (const auto protocol : metricProtocols) {
                meterProvider->AddMetricReader(CreateMetricReader(CreateMetricExporter(localConfig, protocol), localConfig));
            }
        }

        if (!traceProtocols.empty()) {
            auto processors = std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>{};
            for (const auto protocol : traceProtocols) {
                processors.emplace_back(CreateSpanProcessor(CreateSpanExporter(localConfig, protocol), protocol));
            }
            tracerProvider = std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>{
                opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processors), resource)};
        }

        Shutdown(localConfig.timeout_ms);
        {
            auto &state = State();
            std::lock_guard lock{state.mutex};
            state.loggerProvider = loggerProvider;
            state.meterProvider = meterProvider;
            state.tracerProvider = tracerProvider;
            state.meter = {};
            state.tracer = {};
            if (meterProvider) {
                state.meter = meterProvider->GetMeter("nestdaq", std::string{NESTDAQ_VERSION});
                ConfigureProcessMetrics(state);
                ConfigureFairMQThroughputMetrics(state);
            }
            if (tracerProvider) {
                state.tracer = tracerProvider->GetTracer("nestdaq", std::string{NESTDAQ_VERSION});
            }
            state.lastError.clear();
        }

        if (loggerProvider) {
            opentelemetry::logs::Provider::SetLoggerProvider(
                opentelemetry::nostd::shared_ptr<opentelemetry::logs::LoggerProvider>{
                    std::shared_ptr<opentelemetry::logs::LoggerProvider>{loggerProvider}});
            FairLoggerOpenTelemetrySink::SetMinSeverity(localConfig.min_severity);
            FairLoggerOpenTelemetrySink::Initialize();
            LOG(info) << NestDAQMetadataLogBody();
            LOG(info) << FairMQMetadataLogBody(localConfig);
        }
        if (meterProvider) {
            opentelemetry::metrics::Provider::SetMeterProvider(
                opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>{
                    std::shared_ptr<opentelemetry::metrics::MeterProvider>{meterProvider}});
        }
        if (tracerProvider) {
            opentelemetry::trace::Provider::SetTracerProvider(
                opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>{
                    std::shared_ptr<opentelemetry::trace::TracerProvider>{tracerProvider}});
        }
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry initialization error");
    }
}

auto OpenTelemetryInitializer::LastError() noexcept -> const char *
{
    auto &state = State();
    std::lock_guard lock{state.mutex};
    return state.lastError.data();
}

auto OpenTelemetryInitializer::MetricAddDoubleCounter(const char *name,
                                                      double value,
                                                      const char *unit,
                                                      const char *description,
                                                      const nestdaq_otel_attribute *attributes,
                                                      uint64_t attribute_count) -> int
{
    if (IsEmpty(name)) {
        return SetLastError("metric counter name is empty");
    }
    auto attrs = BuildAttributes(attributes, attribute_count);
    auto &state = State();
    std::lock_guard lock{state.mutex};
    if (!state.meter) {
        state.lastError.clear();
        return NESTDAQ_OTEL_OK;
    }
    auto key = MetricKey{.kind = MetricKind::DoubleCounter,
                         .name = name,
                         .unit = IsEmpty(unit) ? "" : unit,
                         .description = IsEmpty(description) ? "" : description};
    auto &counter = state.doubleCounters[key];
    if (!counter) {
        counter = state.meter->CreateDoubleCounter(key.name, key.description, key.unit);
    }
    auto view = opentelemetry::common::KeyValueIterableView<decltype(attrs.values)>{attrs.values};
    counter->Add(value, view);
    state.lastError.clear();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::MetricRecordDoubleHistogram(const char *name,
                                                           double value,
                                                           const char *unit,
                                                           const char *description,
                                                           const nestdaq_otel_attribute *attributes,
                                                           uint64_t attribute_count) -> int
{
    if (IsEmpty(name)) {
        return SetLastError("metric histogram name is empty");
    }
    auto attrs = BuildAttributes(attributes, attribute_count);
    auto &state = State();
    std::lock_guard lock{state.mutex};
    if (!state.meter) {
        state.lastError.clear();
        return NESTDAQ_OTEL_OK;
    }
    auto key = MetricKey{.kind = MetricKind::DoubleHistogram,
                         .name = name,
                         .unit = IsEmpty(unit) ? "" : unit,
                         .description = IsEmpty(description) ? "" : description};
    auto &histogram = state.doubleHistograms[key];
    if (!histogram) {
        histogram = state.meter->CreateDoubleHistogram(key.name, key.description, key.unit);
    }
    auto view = opentelemetry::common::KeyValueIterableView<decltype(attrs.values)>{attrs.values};
    histogram->Record(value, view, opentelemetry::context::Context{});
    state.lastError.clear();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::MetricRecordDoubleGauge(const char *name,
                                                       double value,
                                                       const char *unit,
                                                       const char *description,
                                                       const nestdaq_otel_attribute *attributes,
                                                       uint64_t attribute_count) -> int
{
    if (IsEmpty(name)) {
        return SetLastError("metric gauge name is empty");
    }
    auto gaugeAttributes = BuildGaugeAttributes(attributes, attribute_count);
    auto key = MetricKey{.kind = MetricKind::DoubleGauge,
                         .name = name,
                         .unit = IsEmpty(unit) ? "" : unit,
                         .description = IsEmpty(description) ? "" : description};

    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument> newGauge;
    MetricKey *callbackKey = nullptr;
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        if (!state.meter) {
            state.lastError.clear();
            return NESTDAQ_OTEL_OK;
        }

        auto [gauge, _] = state.doubleGauges.try_emplace(key);
        if (!gauge->second.instrument) {
            gauge->second.callbackKey = key;
            gauge->second.instrument = state.meter->CreateDoubleObservableGauge(key.name, key.description, key.unit);
            if (gauge->second.instrument) {
                newGauge = gauge->second.instrument;
                callbackKey = &gauge->second.callbackKey;
            }
        }

        state.doubleGaugeMeasurements[GaugeSampleKey{.metric = std::move(key),
                                                     .attributes = std::move(gaugeAttributes)}] = value;
        state.lastError.clear();
    }
    if (newGauge) {
        newGauge->AddCallback(ObserveUserDoubleGauge, callbackKey);
    }
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::RecordFairMQThroughput(const telemetry::FairMQThroughputSample &sample) noexcept
-> void
{
    try {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        if (!state.meter) {
            return;
        }
        state.fairmqThroughputMeasurements[{sample.subChannelName, "in"}] = FairMQThroughputMeasurement{
            .channelName = sample.channelName,
            .subChannelName = sample.subChannelName,
            .direction = "in",
            .subChannelIndex = sample.subChannelIndex,
            .messagesPerSecond = sample.messagesPerSecondIn,
            .megabytesPerSecond = sample.megabytesPerSecondIn,
        };
        state.fairmqThroughputMeasurements[{sample.subChannelName, "out"}] = FairMQThroughputMeasurement{
            .channelName = sample.channelName,
            .subChannelName = sample.subChannelName,
            .direction = "out",
            .subChannelIndex = sample.subChannelIndex,
            .messagesPerSecond = sample.messagesPerSecondOut,
            .megabytesPerSecond = sample.megabytesPerSecondOut,
        };
    } catch (...) {
    }
}

auto OpenTelemetryInitializer::SetNestdaqInstanceId(const char *instance_id) -> int
{
    FairLoggerOpenTelemetrySink::SetNestdaqInstanceId(IsEmpty(instance_id) ? "" : instance_id);
    ClearLastError();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::SetMinSeverity(int32_t severity) -> int
{
    if (!ValidateSeverity(severity)) {
        return SetLastError("severity must be a valid fair::Severity numeric value");
    }
    FairLoggerOpenTelemetrySink::SetMinSeverity(severity);
    ClearLastError();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::Shutdown(uint64_t timeout_ms) -> int
{
    try {
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> loggerProvider;
        std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meterProvider;
        std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracerProvider;
        {
            auto &state = State();
            std::lock_guard lock{state.mutex};
            loggerProvider = std::move(state.loggerProvider);
            meterProvider = std::move(state.meterProvider);
            tracerProvider = std::move(state.tracerProvider);
            state.loggerProvider.reset();
            state.meterProvider.reset();
            state.tracerProvider.reset();
            state.meter = {};
            state.tracer = {};
            state.doubleCounters.clear();
            state.doubleHistograms.clear();
            state.doubleGauges.clear();
            state.doubleGaugeMeasurements.clear();
            state.fairmqMessagesPerSecondGauge = {};
            state.fairmqMegabytesPerSecondGauge = {};
            state.processCpuUsageGauge = {};
            state.processMemoryRssGauge = {};
            state.fairmqThroughputMeasurements.clear();
            state.processCpuUsageSample = std::nullopt;
            state.pageSize = 0;
            state.spans.clear();
        }
        FairLoggerOpenTelemetrySink::Shutdown();
        if (loggerProvider) {
            loggerProvider->ForceFlush(TimeoutFromMs(timeout_ms));
            loggerProvider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        if (meterProvider) {
            meterProvider->ForceFlush(TimeoutFromMs(timeout_ms));
            meterProvider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        if (tracerProvider) {
            tracerProvider->ForceFlush(TimeoutFromMs(timeout_ms));
            tracerProvider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        InstallNoopProviders();
        ClearLastError();
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry shutdown error");
    }
}

auto OpenTelemetryInitializer::SpanEnd(uint64_t span_handle) -> int
{
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        auto it = state.spans.find(span_handle);
        if (it == state.spans.end()) {
            return SetLastError("OpenTelemetry span handle is not active");
        }
        span = it->second;
        state.spans.erase(it);
        state.lastError.clear();
    }
    span->End();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::SpanSetAttribute(uint64_t span_handle, const nestdaq_otel_attribute *attribute) -> int
{
    if (!ValidateAttribute(attribute)) {
        return SetLastError("OpenTelemetry span attribute is invalid");
    }
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    {
        auto &state = State();
        std::lock_guard lock{state.mutex};
        auto it = state.spans.find(span_handle);
        if (it == state.spans.end()) {
            return SetLastError("OpenTelemetry span handle is not active");
        }
        span = it->second;
        state.lastError.clear();
    }
    auto storage = AttributeStorage{};
    storage.keys.reserve(1);
    storage.values.reserve(1);
    AppendAttribute(storage, *attribute);
    if (!storage.values.empty()) {
        span->SetAttribute(storage.values.front().first, storage.values.front().second);
    }
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::SpanStart(const char *name,
                                         const nestdaq_otel_attribute *attributes,
                                         uint64_t attribute_count) -> uint64_t
{
    if (IsEmpty(name)) {
        SetLastError("OpenTelemetry span name is empty");
        return 0;
    }
    auto attrs = BuildAttributes(attributes, attribute_count);
    auto &state = State();
    std::lock_guard lock{state.mutex};
    if (!state.tracer) {
        state.lastError.clear();
        return 0;
    }
    auto span = state.tracer->StartSpan(name, attrs.values);
    const auto handle = state.nextSpanHandle.fetch_add(1, std::memory_order_relaxed);
    state.spans.emplace(handle, std::move(span));
    state.lastError.clear();
    return handle;
}

} // namespace nestdaq

extern "C" {

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_force_flush(uint64_t timeout_ms)
    {
        return nestdaq::OpenTelemetryInitializer::ForceFlush(timeout_ms);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_init(const nestdaq_otel_config *config)
    {
        return nestdaq::OpenTelemetryInitializer::Initialize(config);
    }

    NESTDAQ_OTEL_EXPORT const char *nestdaq_otel_last_error(void)
    {
        return nestdaq::OpenTelemetryInitializer::LastError();
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_add_double_counter(const char *name,
                                                                   double value,
                                                                   const char *unit,
                                                                   const char *description,
                                                                   const nestdaq_otel_attribute *attributes,
                                                                   uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::MetricAddDoubleCounter(
            name, value, unit, description, attributes, attribute_count);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_record_double_histogram(const char *name,
                                                                        double value,
                                                                        const char *unit,
                                                                        const char *description,
                                                                        const nestdaq_otel_attribute *attributes,
                                                                        uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::MetricRecordDoubleHistogram(
            name, value, unit, description, attributes, attribute_count);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_metric_record_double_gauge(const char *name,
                                                                    double value,
                                                                    const char *unit,
                                                                    const char *description,
                                                                    const nestdaq_otel_attribute *attributes,
                                                                    uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::MetricRecordDoubleGauge(
            name, value, unit, description, attributes, attribute_count);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_min_severity(int32_t severity)
    {
        return nestdaq::OpenTelemetryInitializer::SetMinSeverity(severity);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_nestdaq_instance_id(const char *instance_id)
    {
        return nestdaq::OpenTelemetryInitializer::SetNestdaqInstanceId(instance_id);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_shutdown(uint64_t timeout_ms)
    {
        return nestdaq::OpenTelemetryInitializer::Shutdown(timeout_ms);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_end(uint64_t span_handle)
    {
        return nestdaq::OpenTelemetryInitializer::SpanEnd(span_handle);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_span_set_attribute(uint64_t span_handle,
                                                           const nestdaq_otel_attribute *attribute)
    {
        return nestdaq::OpenTelemetryInitializer::SpanSetAttribute(span_handle, attribute);
    }

    NESTDAQ_OTEL_EXPORT uint64_t nestdaq_otel_span_start(const char *name,
                                                        const nestdaq_otel_attribute *attributes,
                                                        uint64_t attribute_count)
    {
        return nestdaq::OpenTelemetryInitializer::SpanStart(name, attributes, attribute_count);
    }

}
