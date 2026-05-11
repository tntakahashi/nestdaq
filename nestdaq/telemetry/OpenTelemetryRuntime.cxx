#include "nestdaq/telemetry/OpenTelemetryRuntime.h"

#include <algorithm>
#include <cctype>
#include <string>
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

auto ParseProtocolToken(std::string_view protocol, Protocol &out) -> bool;
auto ToLower(std::string_view value) -> std::string;
auto Trim(std::string_view value) -> std::string_view;

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

auto ToLower(std::string_view value) -> std::string
{
    auto out = std::string{value};
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
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

} // namespace

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

auto TimeoutFromMs(uint64_t timeoutMs) noexcept -> std::chrono::microseconds
{
    if (timeoutMs == 0) {
        return (std::chrono::microseconds::max)();
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds{timeoutMs});
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

} // namespace nestdaq::otel_detail
