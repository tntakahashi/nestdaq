#include "nestdaq/telemetry/OpenTelemetryInitializer.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <opentelemetry/exporters/ostream/log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/logs/noop.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_options.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/provider.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>
#include <opentelemetry/sdk/resource/resource.h>

#include "nestdaq/telemetry/FairLoggerOpenTelemetrySink.h"

#if __has_include("nestdaq/version.h")
#  include "nestdaq/version.h"
#else
static constexpr std::string_view NESTDAQ_VERSION {"unknown"};
#endif

namespace nestdaq {
namespace {

enum class Protocol : std::uint8_t {
    Console,
    OtlpHttp,
    OtlpGrpc,
};

static constexpr std::string_view kDefaultProtocol{"console"};
static constexpr std::string_view kDefaultHttpEndpoint{"http://localhost:4318/v1/logs"};
static constexpr std::string_view kDefaultGrpcEndpoint{"localhost:4317"};
static constexpr int32_t kMaxFairLoggerSeverity{15};

struct RuntimeState {
    std::mutex mutex;
    std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> provider;
    std::string lastError;
};

auto State() -> RuntimeState &
{
    static auto state = RuntimeState{};
    return state;
}

auto SetLastError(std::string message) -> int
{
    auto &state = State();
    std::lock_guard lock{state.mutex};
    state.lastError = std::move(message);
    return NESTDAQ_OTEL_ERROR;
}

auto ClearLastError() -> void
{
    auto &state = State();
    std::lock_guard lock{state.mutex};
    state.lastError.clear();
}

auto IsEmpty(const char *value) noexcept -> bool
{
    return value == nullptr || *value == '\0';
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

auto ParseProtocols(const char *protocols, std::vector<Protocol> &out) -> bool
{
    if (protocols == nullptr) {
        out.emplace_back(Protocol::Console);
        return true;
    }

    auto input = std::string_view{protocols};
    if (input.empty()) {
        return true;
    }

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

auto ValidateSeverity(int32_t severity) noexcept -> bool
{
    return severity >= 0 && severity <= kMaxFairLoggerSeverity;
}

auto TimeoutFromMs(uint64_t timeoutMs) noexcept -> std::chrono::microseconds
{
    if (timeoutMs == 0) {
        return (std::chrono::microseconds::max)();
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::milliseconds{timeoutMs});
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
        input = comma == std::string_view::npos ? std::string_view{} :
                input.substr(comma + 1);

        const auto equals = item.find('=');
        if (equals == std::string_view::npos || equals == 0) {
            continue;
        }
        auto key = item.substr(0, equals);
        auto value = item.substr(equals + 1);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front())) != 0) {
            key.remove_prefix(1);
        }
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back())) != 0) {
            key.remove_suffix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
            value.remove_suffix(1);
        }
        if (!key.empty()) {
            parsed.emplace(std::string{key}, std::string{value});
        }
    }
    return parsed;
}

auto AddStringAttribute(opentelemetry::sdk::resource::ResourceAttributes &attributes,
                        const char *key,
                        const char *value) -> void
{
    if (!IsEmpty(value)) {
        attributes.emplace(key, std::string{value});
    }
}

auto MakeResource(const nestdaq_otel_config_v1 &config) -> opentelemetry::sdk::resource::Resource
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
    AddStringAttribute(attributes, "fairmq.git_version", config.fairmq_git_version);
    AddStringAttribute(attributes, "fairmq.build_type", config.fairmq_build_type);
    AddStringAttribute(attributes, "fairmq.repo_url", config.fairmq_repo_url);
    AddStringAttribute(attributes, "fairmq.license", config.fairmq_license);
    AddStringAttribute(attributes, "fairmq.copyright", config.fairmq_copyright);

    return opentelemetry::sdk::resource::Resource::Create(attributes);
}

auto HttpEndpoint(const nestdaq_otel_config_v1 &config) -> const char *
{
    if (!IsEmpty(config.endpoint_http)) {
        return config.endpoint_http;
    }
    if (!IsEmpty(config.endpoint)) {
        return config.endpoint;
    }
    return kDefaultHttpEndpoint.data();
}

auto GrpcEndpoint(const nestdaq_otel_config_v1 &config) -> const char *
{
    if (!IsEmpty(config.endpoint_grpc)) {
        return config.endpoint_grpc;
    }
    if (!IsEmpty(config.endpoint)) {
        return config.endpoint;
    }
    return kDefaultGrpcEndpoint.data();
}

auto CreateExporter(const nestdaq_otel_config_v1 &config, Protocol protocol)
-> std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
{
    switch (protocol) {
    case Protocol::Console:
        return opentelemetry::exporter::logs::OStreamLogRecordExporterFactory::Create();

    case Protocol::OtlpHttp: {
        auto options = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions{};
        options.url = HttpEndpoint(config);
        if (!IsEmpty(config.headers)) {
            options.http_headers = ParseHeaders(config.headers);
        }
        options.content_type = config.otlp_http_json == 0
                               ? opentelemetry::exporter::otlp::HttpRequestContentType::kBinary
                               : opentelemetry::exporter::otlp::HttpRequestContentType::kJson;
        if (config.timeout_ms != 0) {
            options.timeout = TimeoutFromMs(config.timeout_ms);
        }
        return opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(options);
    }

    case Protocol::OtlpGrpc: {
        auto options = opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterOptions{};
        options.endpoint = GrpcEndpoint(config);
        if (!IsEmpty(config.headers)) {
            options.metadata = ParseHeaders(config.headers);
        }
        if (config.timeout_ms != 0) {
            options.timeout = TimeoutFromMs(config.timeout_ms);
        }
        return opentelemetry::exporter::otlp::OtlpGrpcLogRecordExporterFactory::Create(options);
    }
    }

    return nullptr;
}

auto CreateProcessor(std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter,
                     Protocol protocol) -> std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
{
    if (protocol == Protocol::Console) {
        return opentelemetry::sdk::logs::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
    }

    auto options = opentelemetry::sdk::logs::BatchLogRecordProcessorOptions{};
    return opentelemetry::sdk::logs::BatchLogRecordProcessorFactory::Create(std::move(exporter), options);
}

auto InstallNoopProvider() -> void
{
    opentelemetry::logs::Provider::SetLoggerProvider(
    opentelemetry::nostd::shared_ptr<opentelemetry::logs::LoggerProvider> {
        new opentelemetry::logs::NoopLoggerProvider
    });
}

} // namespace

auto OpenTelemetryInitializer::Initialize(const nestdaq_otel_config_v1 *config) -> int
{
    auto localConfig = nestdaq_otel_config_v1{};
    localConfig.size = sizeof(localConfig);
    localConfig.protocol = kDefaultProtocol.data();
    localConfig.endpoint_http = kDefaultHttpEndpoint.data();
    localConfig.endpoint_grpc = kDefaultGrpcEndpoint.data();
    localConfig.service_name = "nestdaq";
    localConfig.min_severity = static_cast<int32_t>(1);

    if (config != nullptr) {
        if (config->size != sizeof(nestdaq_otel_config_v1)) {
            return SetLastError("nestdaq_otel_config_v1 has an unsupported size");
        }
        localConfig = *config;
        if (IsEmpty(localConfig.service_name)) {
            localConfig.service_name = "nestdaq";
        }
    }

    if (!ValidateSeverity(localConfig.min_severity)) {
        return SetLastError("min_severity must be a fair::Severity numeric value in the range 0..15");
    }

    auto protocols = std::vector<Protocol>{};
    if (!ParseProtocols(localConfig.protocol, protocols)) {
        return SetLastError("unsupported OpenTelemetry protocol; expected comma-separated console, otlp-http, or otlp-grpc");
    }

    try {
        auto processors = std::vector<std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>>{};
        processors.reserve(protocols.size());

        for (const auto protocol : protocols) {
            auto exporter = CreateExporter(localConfig, protocol);
            if (!exporter) {
                return SetLastError("failed to create OpenTelemetry log exporter");
            }

            processors.emplace_back(CreateProcessor(std::move(exporter), protocol));
        }

        if (processors.empty()) {
            {
                auto &state = State();
                std::lock_guard lock{state.mutex};
                if (state.provider) {
                    FairLoggerOpenTelemetrySink::Shutdown();
                    state.provider->Shutdown(TimeoutFromMs(localConfig.timeout_ms));
                    state.provider.reset();
                }
                state.lastError.clear();
            }
            InstallNoopProvider();
            return NESTDAQ_OTEL_OK;
        }

        auto provider = opentelemetry::sdk::logs::LoggerProviderFactory::Create(std::move(processors),
                                                                                MakeResource(localConfig));
        auto sharedProvider = std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> {std::move(provider)};
        auto baseProvider = std::shared_ptr<opentelemetry::logs::LoggerProvider> {sharedProvider};

        {
            auto &state = State();
            std::lock_guard lock{state.mutex};
            if (state.provider) {
                FairLoggerOpenTelemetrySink::Shutdown();
                state.provider->Shutdown(TimeoutFromMs(localConfig.timeout_ms));
                state.provider.reset();
            }
            state.provider = sharedProvider;
            state.lastError.clear();
        }

        opentelemetry::logs::Provider::SetLoggerProvider(
            opentelemetry::nostd::shared_ptr<opentelemetry::logs::LoggerProvider> {baseProvider});
        FairLoggerOpenTelemetrySink::SetMinSeverity(localConfig.min_severity);
        FairLoggerOpenTelemetrySink::Initialize();
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry initialization error");
    }
}

auto OpenTelemetryInitializer::SetMinSeverity(int32_t severity) -> int
{
    if (!ValidateSeverity(severity)) {
        return SetLastError("severity must be a fair::Severity numeric value in the range 0..15");
    }
    FairLoggerOpenTelemetrySink::SetMinSeverity(severity);
    ClearLastError();
    return NESTDAQ_OTEL_OK;
}

auto OpenTelemetryInitializer::ForceFlush(uint64_t timeout_ms) -> int
{
    try {
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> provider;
        {
            auto &state = State();
            std::lock_guard lock{state.mutex};
            provider = state.provider;
        }
        if (!provider) {
            return SetLastError("OpenTelemetry logger provider is not initialized");
        }
        if (!provider->ForceFlush(TimeoutFromMs(timeout_ms))) {
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

auto OpenTelemetryInitializer::Shutdown(uint64_t timeout_ms) -> int
{
    try {
        std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> provider;
        {
            auto &state = State();
            std::lock_guard lock{state.mutex};
            provider = std::move(state.provider);
            state.provider.reset();
        }

        FairLoggerOpenTelemetrySink::Shutdown();
        if (provider) {
            provider->ForceFlush(TimeoutFromMs(timeout_ms));
            provider->Shutdown(TimeoutFromMs(timeout_ms));
        }
        InstallNoopProvider();
        ClearLastError();
        return NESTDAQ_OTEL_OK;
    } catch (const std::exception &ex) {
        return SetLastError(ex.what());
    } catch (...) {
        return SetLastError("unknown OpenTelemetry shutdown error");
    }
}

auto OpenTelemetryInitializer::LastError() noexcept -> const char *
{
    auto &state = State();
    std::lock_guard lock{state.mutex};
    return state.lastError.c_str();
}

} // namespace nestdaq

extern "C" {

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_init_v1(const nestdaq_otel_config_v1 *config)
    {
        return nestdaq::OpenTelemetryInitializer::Initialize(config);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_set_min_severity(int32_t severity)
    {
        return nestdaq::OpenTelemetryInitializer::SetMinSeverity(severity);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_force_flush(uint64_t timeout_ms)
    {
        return nestdaq::OpenTelemetryInitializer::ForceFlush(timeout_ms);
    }

    NESTDAQ_OTEL_EXPORT int nestdaq_otel_shutdown(uint64_t timeout_ms)
    {
        return nestdaq::OpenTelemetryInitializer::Shutdown(timeout_ms);
    }

    NESTDAQ_OTEL_EXPORT const char *nestdaq_otel_last_error(void)
    {
        return nestdaq::OpenTelemetryInitializer::LastError();
    }

}
