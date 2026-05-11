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

inline constexpr std::string_view kDefaultLogProtocol{"console"};
inline constexpr std::string_view kDefaultLogHttpEndpoint{"http://localhost:4318/v1/logs"};
inline constexpr std::string_view kDefaultMetricHttpEndpoint{"http://localhost:4318/v1/metrics"};
inline constexpr std::string_view kDefaultTraceHttpEndpoint{"http://localhost:4318/v1/traces"};
inline constexpr std::string_view kDefaultGrpcEndpoint{"localhost:4317"};
inline constexpr uint32_t kDefaultMetricExportIntervalMs{60000};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
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

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct GaugeSampleKey {
    MetricKey metric;
    std::vector<GaugeAttribute> attributes;

    auto operator<(const GaugeSampleKey &other) const -> bool
    {
        return std::tie(metric, attributes) < std::tie(other.metric, other.attributes);
    }
};

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
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
auto ConfigureFairMQThroughputMetrics(RuntimeState &state) -> void;
auto ConfigureProcessMetrics(RuntimeState &state) -> void;
auto DefaultConfig() -> nestdaq_otel_config;
auto FairMQMetadataLogBody(const nestdaq_otel_config &config) -> std::string;
auto InstallNoopProviders() -> void;
auto IsEmpty(const char *value) noexcept -> bool;
auto MakeResource(const nestdaq_otel_config &config) -> opentelemetry::sdk::resource::Resource;
auto MetadataValue(const char *value) -> std::string;
auto MetadataValue(std::string_view value) -> std::string;
auto NestDAQMetadataLogBody() -> std::string;
auto ParseHeaders(const char *headers) -> opentelemetry::exporter::otlp::OtlpHeaders;
auto ParseProtocols(const char *protocols, std::vector<Protocol> &out) -> bool;
auto SetLastError(std::string message) -> int;
auto SignalEnabled(const nestdaq_otel_signal_config &config) noexcept -> bool;
auto State() -> RuntimeState &;
auto TimeoutFromMs(uint64_t timeoutMs) noexcept -> std::chrono::microseconds;
auto ValidateAttribute(const nestdaq_otel_attribute *attribute) noexcept -> bool;
auto ValidateSeverity(int32_t severity) noexcept -> bool;

} // namespace nestdaq::otel_detail
