/** @file
 *  @brief Implements shared OpenTelemetry plugin runtime helpers.
 */

#include "nestdaq/telemetry/OpenTelemetryRuntime.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>
#include <utility>

#include <nlohmann/json.hpp>

#include <opentelemetry/logs/noop.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/noop.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/trace/noop.h>
#include <opentelemetry/trace/provider.h>

#include <fairlogger/Logger.h>
#include <fairmq/Version.h>

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

namespace nestdaq::otel_detail {
namespace {

auto parseProtocolToken(std::string_view protocol, Protocol &out) -> bool;
auto toLower(std::string_view value) -> std::string;
auto trim(std::string_view value) -> std::string_view;

auto parseProtocolToken(std::string_view protocol, Protocol &out) -> bool
{
    const auto normalized = toLower(protocol);
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

auto toLower(std::string_view value) -> std::string
{
    auto out = std::string{value};
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

auto trim(std::string_view value) -> std::string_view
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

} // namespace

auto addStringAttribute(opentelemetry::sdk::resource::ResourceAttributes &attributes,
                        const char *key,
                        const char *value) -> void
{
    if (!isEmpty(value)) {
        attributes.emplace(key, std::string{value});
    }
}

auto appendAttribute(AttributeStorage &storage, const nestdaq_otel_attribute &attribute) -> void
{
    if (!validateAttribute(&attribute)) {
        return;
    }

    storage.keys.emplace_back(attribute.key);
    auto key = opentelemetry::nostd::string_view{storage.keys.back()};
    switch (attribute.type) {
    case NESTDAQ_OTEL_ATTRIBUTE_STRING:
        storage.values.emplace_back(key, isEmpty(attribute.string_value) ? "" : attribute.string_value);
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

auto buildAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> AttributeStorage
{
    auto storage = AttributeStorage{};
    storage.keys.reserve(attributeCount);
    storage.values.reserve(attributeCount);
    if (attributes == nullptr) {
        return storage;
    }
    for (uint64_t i = 0; i < attributeCount; ++i) {
        appendAttribute(storage, attributes[i]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return storage;
}

auto buildGaugeAttributes(const nestdaq_otel_attribute *attributes, uint64_t attributeCount) -> std::vector<GaugeAttribute>
{
    auto values = std::vector<GaugeAttribute> {};
    values.reserve(attributeCount);
    if (attributes == nullptr) {
        return values;
    }

    for (uint64_t i = 0; i < attributeCount; ++i) {
        const auto &attribute = attributes[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (!validateAttribute(&attribute)) {
            continue;
        }

        auto value = GaugeAttribute{};
        value.key = attribute.key;
        value.type = attribute.type;
        switch (attribute.type) {
        case NESTDAQ_OTEL_ATTRIBUTE_STRING:
            value.stringValue = isEmpty(attribute.string_value) ? "" : attribute.string_value;
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

auto clearLastError() -> void
{
    auto &state = runtimeState();
    std::scoped_lock lock{state.mutex};
    state.lastError.clear();
}

auto flushFrameworkMetricsIfDirty(uint64_t timeoutMs) -> int
{
    std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> frameworkMeterProvider;
    auto throughputCount = std::size_t{0};
    auto processCount = std::size_t{0};
    auto stateCount = std::size_t{0};
    {
        auto &state = runtimeState();
        std::scoped_lock lock{state.mutex};
        if (state.pendingFairMQThroughputMeasurements.empty() &&
                state.pendingProcessUsageMeasurements.empty() &&
                state.pendingFairMQStateMeasurements.empty()) {
            state.lastError.clear();
            return NESTDAQ_OTEL_OK;
        }
        frameworkMeterProvider = state.frameworkMeterProvider;
        if (!frameworkMeterProvider) {
            state.lastError.clear();
            return NESTDAQ_OTEL_OK;
        }
        state.exportingFairMQThroughputMeasurements = state.pendingFairMQThroughputMeasurements;
        state.exportingProcessUsageMeasurements = state.pendingProcessUsageMeasurements;
        state.exportingFairMQStateMeasurements = state.pendingFairMQStateMeasurements;
        throughputCount = state.exportingFairMQThroughputMeasurements.size();
        processCount = state.exportingProcessUsageMeasurements.size();
        stateCount = state.exportingFairMQStateMeasurements.size();
    }

    // Export a snapshot of pending framework samples. Successful flushes erase
    // only the exported prefix and recreate observable instruments so already
    // exported one-shot samples cannot be observed again.
    const auto ok = frameworkMeterProvider->ForceFlush(timeoutFromMs(timeoutMs));
    auto shouldRecreateProvider = false;
    auto &state = runtimeState();
    if (ok) {
        std::scoped_lock reconfigureLock{state.frameworkReconfigureMutex};
        {
            std::scoped_lock lock{state.mutex};
            state.exportingFairMQThroughputMeasurements.clear();
            state.exportingProcessUsageMeasurements.clear();
            state.exportingFairMQStateMeasurements.clear();
            state.pendingFairMQThroughputMeasurements.erase(
                state.pendingFairMQThroughputMeasurements.begin(),
                state.pendingFairMQThroughputMeasurements.begin() +
                std::min(throughputCount, state.pendingFairMQThroughputMeasurements.size()));
            state.pendingProcessUsageMeasurements.erase(
                state.pendingProcessUsageMeasurements.begin(),
                state.pendingProcessUsageMeasurements.begin() +
                std::min(processCount, state.pendingProcessUsageMeasurements.size()));
            state.pendingFairMQStateMeasurements.erase(
                state.pendingFairMQStateMeasurements.begin(),
                state.pendingFairMQStateMeasurements.begin() +
                std::min(stateCount, state.pendingFairMQStateMeasurements.size()));
            if (state.frameworkMeterProvider == frameworkMeterProvider) {
                state.frameworkMeterProvider.reset();
                state.frameworkMeter = {};
                state.fairmqMessagesPerSecondGauge = {};
                state.fairmqMegabytesPerSecondGauge = {};
                state.processCpuTimeCounter = {};
                state.processCpuUtilizationGauge = {};
                state.processMemoryUsageCounter = {};
                state.fairmqStateGauge = {};
                shouldRecreateProvider = true;
            }
            state.lastError.clear();
        }
        if (shouldRecreateProvider) {
            configureFrameworkMetricsProvider(state);
        }
        return NESTDAQ_OTEL_OK;
    }
    {
        std::scoped_lock lock{state.mutex};
        state.exportingFairMQThroughputMeasurements.clear();
        state.exportingProcessUsageMeasurements.clear();
        state.exportingFairMQStateMeasurements.clear();
    }
    return setLastError("OpenTelemetry framework metrics force flush failed");
}

auto defaultConfig() -> nestdaq_otel_config
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
    config.nestdaq_instance_id_status = "unresolved";
    config.min_severity = static_cast<int32_t>(fair::Severity::trace);
    config.timeout_ms = 5000;
    config.metric_export_interval_ms = kDefaultMetricExportIntervalMs;
    return config;
}

auto fairMQMetadataLogBody(const nestdaq_otel_config &config) -> std::string
{
    const auto body = nlohmann::json{
        {   "fairmq", {
                {   "version", {
                        {"string", metadataValue(FAIRMQ_VERSION)},
                        {"major", FAIRMQ_VERSION_MAJOR},
                        {"minor", FAIRMQ_VERSION_MINOR},
                        {"patch", FAIRMQ_VERSION_PATCH},
                        {"git", metadataValue(config.fairmq_git_version)},
                    }
                },
                {   "build", {
                        {"type", metadataValue(config.fairmq_build_type)},
                    }
                },
                {   "source", {
                        {"repo_url", metadataValue(config.fairmq_repo_url)},
                    }
                },
                {"license", metadataValue(config.fairmq_license)},
                {"copyright", metadataValue(config.fairmq_copyright)},
            }
        },
    };
    return body.dump();
}

auto installNoopProviders() -> void
{
    opentelemetry::logs::Provider::SetLoggerProvider(
    opentelemetry::nostd::shared_ptr<opentelemetry::logs::LoggerProvider> {
        new opentelemetry::logs::NoopLoggerProvider
    });
    opentelemetry::metrics::Provider::SetMeterProvider(
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider> {
        new opentelemetry::metrics::NoopMeterProvider
    });
    opentelemetry::trace::Provider::SetTracerProvider(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider> {
        new opentelemetry::trace::NoopTracerProvider
    });
}

auto isEmpty(const char *value) noexcept -> bool
{
    return value == nullptr || *value == '\0';
}

auto makeResource(const nestdaq_otel_config &config) -> opentelemetry::sdk::resource::Resource
{
    auto attributes = opentelemetry::sdk::resource::ResourceAttributes{};
    attributes.emplace("service.name", std::string{isEmpty(config.service_name) ? "nestdaq" : config.service_name});
    attributes.emplace("service.version", std::string{NESTDAQ_VERSION});
    addStringAttribute(attributes, "service.namespace", config.service_namespace);
    addStringAttribute(attributes, "service.instance.id", config.service_instance_id);
    addStringAttribute(attributes, "host.name", config.host_name);
    addStringAttribute(attributes, "nestdaq.instance.id", config.nestdaq_instance_id);
    addStringAttribute(attributes, "nestdaq.instance.id.status", config.nestdaq_instance_id_status);
    addStringAttribute(attributes, "fairmq.id", config.fairmq_id);
    addStringAttribute(attributes, "fairmq.device", config.fairmq_device);
    addStringAttribute(attributes, "fairmq.session", config.fairmq_session);
    addStringAttribute(attributes, "fairmq.transport", config.fairmq_transport);
    return opentelemetry::sdk::resource::Resource::Create(attributes);
}

auto metadataValue(const char *value) -> std::string
{
    return isEmpty(value) ? std::string{"unknown"} :
           std::string{value};
}

auto metadataValue(std::string_view value) -> std::string
{
    return value.empty() ? std::string{"unknown"} :
           std::string{value};
}

auto nestDAQMetadataLogBody() -> std::string
{
    const auto body = nlohmann::json{
        {   "nestdaq", {
                {   "version", {
                        {"string", metadataValue(NESTDAQ_VERSION)},
                        {"major", NESTDAQ_VERSION_MAJOR},
                        {"minor", NESTDAQ_VERSION_MINOR},
                        {"patch", NESTDAQ_VERSION_PATCH},
                        {"prerelease", std::string{NESTDAQ_VERSION_PRERELEASE}},
                    }
                },
                {   "build", {
                        {"type", metadataValue(NESTDAQ_BUILD_TYPE)},
                    }
                },
                {   "git", {
                        {"commit_count", NESTDAQ_GIT_COMMIT_COUNT},
                        {"commit_hash", metadataValue(NESTDAQ_GIT_COMMIT_HASH_STRING)},
                        {"branch", metadataValue(NESTDAQ_GIT_BRANCH)},
                        {"remote_url", metadataValue(NESTDAQ_GIT_REMOTE_URL)},
                        {"commit_date", metadataValue(NESTDAQ_GIT_COMMIT_DATE)},
                    }
                },
            }
        },
    };
    return body.dump();
}

auto parseHeaders(const char *headers) -> opentelemetry::exporter::otlp::OtlpHeaders
{
    auto parsed = opentelemetry::exporter::otlp::OtlpHeaders{};
    if (isEmpty(headers)) {
        return parsed;
    }
    auto input = std::string_view{headers};
    while (!input.empty()) {
        const auto comma = input.find(',');
        auto item = input.substr(0, comma);
        input = comma == std::string_view::npos ? std::string_view{} :
                input.substr(comma + 1);
        const auto equals = item.find('=');
        if (equals == std::string_view::npos || equals == 0) {
            continue;
        }
        auto key = trim(item.substr(0, equals));
        auto value = trim(item.substr(equals + 1));
        if (!key.empty()) {
            parsed.emplace(std::string{key}, std::string{value});
        }
    }
    return parsed;
}

auto parseProtocols(const char *protocols, std::vector<Protocol> &out) -> bool
{
    if (protocols == nullptr) {
        return true;
    }
    auto input = std::string_view{protocols};
    while (!input.empty()) {
        const auto comma = input.find(',');
        auto token = trim(input.substr(0, comma));
        input = comma == std::string_view::npos ? std::string_view{} :
                input.substr(comma + 1);
        if (token.empty()) {
            continue;
        }
        auto protocol = Protocol::Console;
        if (!parseProtocolToken(token, protocol)) {
            return false;
        }
        out.emplace_back(protocol);
    }
    return true;
}

auto setLastError(std::string message) -> int
{
    auto &state = runtimeState();
    std::scoped_lock lock{state.mutex};
    state.lastError = std::move(message);
    return NESTDAQ_OTEL_ERROR;
}

auto signalEnabled(const nestdaq_otel_signal_config &config) noexcept -> bool
{
    return !isEmpty(config.protocol);
}

auto storeFrameworkMetricConfig(RuntimeState &state,
                                const nestdaq_otel_config &config,
                                const std::vector<Protocol> &protocols,
                                opentelemetry::sdk::resource::Resource resource) -> void
{
    state.frameworkMetricProtocols.assign(protocols.begin(), protocols.end());
    state.frameworkMetricConfig.metrics.protocol = isEmpty(config.metrics.protocol) ? "" : config.metrics.protocol;
    state.frameworkMetricConfig.metrics.endpointHttp =
        isEmpty(config.metrics.endpoint_http) ? "" : config.metrics.endpoint_http;
    state.frameworkMetricConfig.metrics.endpointGrpc =
        isEmpty(config.metrics.endpoint_grpc) ? "" : config.metrics.endpoint_grpc;
    state.frameworkMetricConfig.metrics.headers = isEmpty(config.metrics.headers) ? "" : config.metrics.headers;
    state.frameworkMetricConfig.metrics.otlpHttpJson = config.metrics.otlp_http_json;
    state.frameworkMetricConfig.timeoutMs = config.timeout_ms;
    state.frameworkMetricConfig.metricExportIntervalMs = config.metric_export_interval_ms;
    state.frameworkMetricResource = std::move(resource);
}

auto runtimeState() -> RuntimeState &
{
    static auto gRuntimeState = RuntimeState{};
    return gRuntimeState;
}

auto timeoutFromMs(uint64_t timeoutMs) noexcept -> std::chrono::microseconds
{
    if (timeoutMs == 0) {
        return (std::chrono::microseconds::max)();
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds{timeoutMs});
}

auto validateAttribute(const nestdaq_otel_attribute *attribute) noexcept -> bool
{
    return attribute != nullptr && !isEmpty(attribute->key);
}

auto validateSeverity(int32_t severity) noexcept -> bool
{
    return severity >= static_cast<int32_t>(fair::Severity::nolog) &&
           static_cast<size_t>(severity) < fair::Logger::fSeverityNames.size();
}

} // namespace nestdaq::otel_detail
