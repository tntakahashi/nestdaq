#pragma once

#include <fairmq/ProgOptions.h>
#include <fairmq/Version.h>
#include <fairlogger/Logger.h>

#include <boost/program_options.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <nestdaq/telemetry/OpenTelemetryInitializer.h>

#include <dlfcn.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace nestdaq::telemetry {

/** Default soname loaded by NestDAQ executables when telemetry is enabled. */
static constexpr std::string_view kDefaultTelemetryLibrary{"libnestdaq_otel.so"};
static constexpr std::string_view kDefaultProtocol{"console"};
static constexpr std::string_view kDefaultLogHttpEndpoint{"http://localhost:4318/v1/logs"};
static constexpr std::string_view kDefaultMetricHttpEndpoint{"http://localhost:4318/v1/metrics"};
static constexpr std::string_view kDefaultTraceHttpEndpoint{"http://localhost:4318/v1/traces"};
static constexpr std::string_view kDefaultGrpcEndpoint{"localhost:4317"};
static constexpr uint32_t kDefaultTimeoutMs{5000};
static constexpr uint32_t kDefaultMetricExportIntervalMs{60000};
static constexpr std::string_view kTelemetryConfigSubscriber{"nestdaq-telemetry"};
static constexpr std::string_view kDefaultServiceNamespace{"nestdaq"};

/**
 * @brief Runtime options used to configure the telemetry plugin.
 *
 * Values are initialized from defaults, optionally overridden by environment
 * variables and command-line/FairMQ options. `MakeConfig()` returns borrowed
 * pointers into this object, so the object must outlive the call to
 * `TelemetryLibrary::InitializeWith()`.
 */
struct TelemetryOptions {
    std::string library{kDefaultTelemetryLibrary};
    std::string logProtocol{kDefaultProtocol};
    std::string metricProtocol;
    std::string traceProtocol;
    std::string logEndpointHttp{kDefaultLogHttpEndpoint};
    std::string logEndpointGrpc{kDefaultGrpcEndpoint};
    std::string metricEndpointHttp{kDefaultMetricHttpEndpoint};
    std::string metricEndpointGrpc{kDefaultGrpcEndpoint};
    std::string traceEndpointHttp{kDefaultTraceHttpEndpoint};
    std::string traceEndpointGrpc{kDefaultGrpcEndpoint};
    std::string logHeaders;
    std::string metricHeaders;
    std::string traceHeaders;
    std::string severity{"info"};
    std::string serviceName{"nestdaq"};
    std::string serviceNamespace{kDefaultServiceNamespace};
    std::string serviceInstanceId;
    std::string fairmqId;
    std::string fairmqDevice;
    std::string fairmqSession;
    std::string fairmqTransport;
    uint32_t timeoutMs{kDefaultTimeoutMs};
    uint32_t metricExportIntervalMs{kDefaultMetricExportIntervalMs};
    uint32_t logOtlpHttpJson{1};
    uint32_t metricOtlpHttpJson{1};
    uint32_t traceOtlpHttpJson{1};
    bool required{false};
    bool generatedServiceInstanceId{false};
};

struct SeverityParseResult {
    int32_t value{static_cast<int32_t>(fair::Severity::info)};
    bool usedFallback{false};
};

class TelemetryLibrary;

/**
 * @brief Add telemetry-related Boost.Program_options entries.
 *
 * The options configure the runtime-loaded telemetry library, per-signal
 * exporters, service resource attributes, and FairLogger severity threshold.
 */
inline auto AddTelemetryOptions(boost::program_options::options_description& options,
                                std::string_view defaultServiceName = "nestdaq") -> void;
/** Apply `NESTDAQ_OTEL_*` environment variables to @p options. */
inline auto ApplyEnvironment(TelemetryOptions& options) -> void;
inline auto AssignOption(TelemetryOptions& options,
                         std::string_view key,
                         std::string_view value) -> void;
inline auto Basename(std::string_view path) -> std::string_view;
inline auto Env(const char* name) -> const char*;
inline auto EnsureServiceInstanceId(TelemetryOptions& options) -> void;
inline auto GenerateUuidString() -> std::string;
/**
 * @brief Build the C ABI config passed to `libnestdaq_otel.so`.
 *
 * The returned struct contains pointers into @p options.
 */
inline auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config;
inline auto MakeSignalConfig(std::string_view protocol,
                             std::string_view endpointHttp,
                             std::string_view endpointGrpc,
                             std::string_view headers,
                             uint32_t otlpHttpJson) -> nestdaq_otel_signal_config;
inline auto ParseBool(std::string_view value) -> bool;
inline auto ParseTelemetryOptions(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                                  std::string_view defaultServiceName = "nestdaq") -> TelemetryOptions;
inline auto ParseFairLoggerSeverity(std::string_view severity) -> SeverityParseResult;
inline auto ParseUInt32(std::string_view value, uint32_t fallback) -> uint32_t;
inline auto ReadTelemetryOptions(const boost::program_options::variables_map& vm,
                                 std::string_view defaultServiceName) -> TelemetryOptions;
inline auto WarnUnknownSeverityFallback(std::string_view severity) -> void;
inline auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t;
inline auto SetGeneratedUuidProperty(fair::mq::ProgOptions& config,
                                     const TelemetryOptions& options,
                                     std::string_view key = "uuid") -> void;
inline auto SubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config,
                                            TelemetryLibrary& telemetry) -> void;
inline auto UnsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void;

inline auto AddTelemetryOptions(boost::program_options::options_description& options,
                                std::string_view defaultServiceName) -> void
{
    namespace bpo = boost::program_options;
    options.add_options()
           ("otel-library", bpo::value<std::string>()->default_value(std::string{kDefaultTelemetryLibrary}), "Telemetry shared library path or soname to dlopen")
           ("otel-log-protocol", bpo::value<std::string>()->default_value(std::string{kDefaultProtocol})->implicit_value(""), "Comma-separated OTel log exporter protocols to enable: console, otlp-http, otlp-grpc. Empty disables OTel output")
           ("otel-metric-protocol", bpo::value<std::string>()->default_value("")->implicit_value(""), "Comma-separated OTel metric exporter protocols to enable: console, otlp-http, otlp-grpc. Empty disables metrics")
           ("otel-trace-protocol", bpo::value<std::string>()->default_value("")->implicit_value(""), "Comma-separated OTel trace exporter protocols to enable: console, otlp-http, otlp-grpc. Empty disables traces")
           ("otel-log-endpoint-http", bpo::value<std::string>()->default_value(std::string{kDefaultLogHttpEndpoint}), "OTLP HTTP logs endpoint")
           ("otel-log-endpoint-grpc", bpo::value<std::string>()->default_value(std::string{kDefaultGrpcEndpoint}), "OTLP gRPC logs endpoint")
           ("otel-metric-endpoint-http", bpo::value<std::string>()->default_value(std::string{kDefaultMetricHttpEndpoint}), "OTLP HTTP metrics endpoint")
           ("otel-metric-endpoint-grpc", bpo::value<std::string>()->default_value(std::string{kDefaultGrpcEndpoint}), "OTLP gRPC metrics endpoint")
           ("otel-trace-endpoint-http", bpo::value<std::string>()->default_value(std::string{kDefaultTraceHttpEndpoint}), "OTLP HTTP traces endpoint")
           ("otel-trace-endpoint-grpc", bpo::value<std::string>()->default_value(std::string{kDefaultGrpcEndpoint}), "OTLP gRPC traces endpoint")
           ("otel-log-headers", bpo::value<std::string>(), "OTel exporter headers as comma-separated key=value pairs")
           ("otel-metric-headers", bpo::value<std::string>(), "OTel metric exporter headers as comma-separated key=value pairs")
           ("otel-trace-headers", bpo::value<std::string>(), "OTel trace exporter headers as comma-separated key=value pairs")
           ("otel-log-severity", bpo::value<std::string>()->default_value("info"), "Minimum severity exported to OTel")
           ("otel-log-required", bpo::value<bool>()->default_value(false), "Fail startup if telemetry library cannot be loaded")
           ("otel-timeout-ms", bpo::value<uint32_t>()->default_value(kDefaultTimeoutMs), "OTel force-flush/shutdown timeout in milliseconds")
           ("otel-metric-export-interval-ms", bpo::value<uint32_t>()->default_value(kDefaultMetricExportIntervalMs), "OTel periodic metric export interval in milliseconds")
           ("otel-log-http-json", bpo::value<bool>()->default_value(true), "Use JSON content type for OTLP HTTP logs")
           ("otel-metric-http-json", bpo::value<bool>()->default_value(true), "Use JSON content type for OTLP HTTP metrics")
           ("otel-trace-http-json", bpo::value<bool>()->default_value(true), "Use JSON content type for OTLP HTTP traces")
           ("otel-service-name", bpo::value<std::string>()->default_value(std::string{defaultServiceName}), "OTel service.name resource attribute")
           ("otel-service-namespace", bpo::value<std::string>()->default_value(std::string{kDefaultServiceNamespace}), "OTel service.namespace resource attribute")
           ("otel-service-instance-id", bpo::value<std::string>(), "OTel service.instance.id resource attribute")
           ("otel-fairmq-id", bpo::value<std::string>(), "FairMQ id resource attribute")
           ("otel-fairmq-device", bpo::value<std::string>(), "FairMQ device resource attribute")
           ("otel-fairmq-session", bpo::value<std::string>(), "FairMQ session resource attribute")
           ("otel-fairmq-transport", bpo::value<std::string>(), "FairMQ transport resource attribute");
}

inline auto ApplyEnvironment(TelemetryOptions& options) -> void
{
    if (const auto* value = Env("NESTDAQ_OTEL_LIBRARY")) {
        options.library = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_PROTOCOL")) {
        options.logProtocol = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_METRIC_PROTOCOL")) {
        options.metricProtocol = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_TRACE_PROTOCOL")) {
        options.traceProtocol = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_ENDPOINT_HTTP")) {
        options.logEndpointHttp = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_ENDPOINT_GRPC")) {
        options.logEndpointGrpc = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_METRIC_ENDPOINT_HTTP")) {
        options.metricEndpointHttp = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_METRIC_ENDPOINT_GRPC")) {
        options.metricEndpointGrpc = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_TRACE_ENDPOINT_HTTP")) {
        options.traceEndpointHttp = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_TRACE_ENDPOINT_GRPC")) {
        options.traceEndpointGrpc = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_HEADERS")) {
        options.logHeaders = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_METRIC_HEADERS")) {
        options.metricHeaders = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_TRACE_HEADERS")) {
        options.traceHeaders = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_SEVERITY")) {
        options.severity = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_REQUIRED")) {
        options.required = ParseBool(value);
    }
}

inline auto AssignOption(TelemetryOptions& options,
                         std::string_view key, // NOLINT(bugprone-easily-swappable-parameters)
                         std::string_view value) -> void
{
    if (key == "otel-library") {
        options.library = value;
    } else if (key == "otel-log-protocol") {
        options.logProtocol = value;
    } else if (key == "otel-metric-protocol") {
        options.metricProtocol = value;
    } else if (key == "otel-trace-protocol") {
        options.traceProtocol = value;
    } else if (key == "otel-log-endpoint-http") {
        options.logEndpointHttp = value;
    } else if (key == "otel-log-endpoint-grpc") {
        options.logEndpointGrpc = value;
    } else if (key == "otel-metric-endpoint-http") {
        options.metricEndpointHttp = value;
    } else if (key == "otel-metric-endpoint-grpc") {
        options.metricEndpointGrpc = value;
    } else if (key == "otel-trace-endpoint-http") {
        options.traceEndpointHttp = value;
    } else if (key == "otel-trace-endpoint-grpc") {
        options.traceEndpointGrpc = value;
    } else if (key == "otel-log-headers") {
        options.logHeaders = value;
    } else if (key == "otel-metric-headers") {
        options.metricHeaders = value;
    } else if (key == "otel-trace-headers") {
        options.traceHeaders = value;
    } else if (key == "otel-log-severity") {
        options.severity = value;
    } else if (key == "otel-log-required") {
        options.required = ParseBool(value);
    } else if (key == "otel-timeout-ms") {
        options.timeoutMs = ParseUInt32(value, options.timeoutMs);
    } else if (key == "otel-metric-export-interval-ms") {
        options.metricExportIntervalMs = ParseUInt32(value, options.metricExportIntervalMs);
    } else if (key == "otel-log-http-json") {
        options.logOtlpHttpJson = ParseBool(value) ? 1U : 0U;
    } else if (key == "otel-metric-http-json") {
        options.metricOtlpHttpJson = ParseBool(value) ? 1U : 0U;
    } else if (key == "otel-trace-http-json") {
        options.traceOtlpHttpJson = ParseBool(value) ? 1U : 0U;
    } else if (key == "otel-service-name") {
        options.serviceName = value;
    } else if (key == "otel-service-namespace") {
        options.serviceNamespace = value;
    } else if (key == "otel-service-instance-id") {
        options.serviceInstanceId = value;
    } else if (key == "otel-fairmq-id") {
        options.fairmqId = value;
    } else if (key == "otel-fairmq-device") {
        options.fairmqDevice = value;
    } else if (key == "otel-fairmq-session") {
        options.fairmqSession = value;
    } else if (key == "otel-fairmq-transport") {
        options.fairmqTransport = value;
    }
}

inline auto Basename(std::string_view path) -> std::string_view
{
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string_view::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

inline auto Env(const char* name) -> const char*
{
    return std::getenv(name); // NOLINT(concurrency-mt-unsafe)
}

inline auto EnsureServiceInstanceId(TelemetryOptions& options) -> void
{
    if (!options.serviceInstanceId.empty()) {
        return;
    }
    options.serviceInstanceId = GenerateUuidString();
    options.generatedServiceInstanceId = true;
}

inline auto GenerateUuidString() -> std::string
{
    return boost::uuids::to_string(boost::uuids::random_generator{}());
}

inline auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config
{
    nestdaq_otel_config config{};
    config.size = sizeof(config);
    config.logs = MakeSignalConfig(options.logProtocol,
                                   options.logEndpointHttp,
                                   options.logEndpointGrpc,
                                   options.logHeaders,
                                   options.logOtlpHttpJson);
    config.metrics = MakeSignalConfig(options.metricProtocol,
                                      options.metricEndpointHttp,
                                      options.metricEndpointGrpc,
                                      options.metricHeaders,
                                      options.metricOtlpHttpJson);
    config.traces = MakeSignalConfig(options.traceProtocol,
                                     options.traceEndpointHttp,
                                     options.traceEndpointGrpc,
                                     options.traceHeaders,
                                     options.traceOtlpHttpJson);
    config.service_name = options.serviceName.data();
    config.service_namespace = options.serviceNamespace.data();
    config.service_instance_id = options.serviceInstanceId.data();
    config.fairmq_id = options.fairmqId.data();
    config.fairmq_device = options.fairmqDevice.data();
    config.fairmq_session = options.fairmqSession.data();
    config.fairmq_transport = options.fairmqTransport.data();
    config.fairmq_git_version = FAIRMQ_GIT_VERSION;
    config.fairmq_build_type = FAIRMQ_BUILD_TYPE;
    config.fairmq_repo_url = FAIRMQ_REPO_URL;
    config.fairmq_license = FAIRMQ_LICENSE;
    config.fairmq_copyright = FAIRMQ_COPYRIGHT;
    config.min_severity = ParseFairLoggerSeverity(options.severity).value;
    config.timeout_ms = options.timeoutMs;
    config.metric_export_interval_ms = options.metricExportIntervalMs;
    return config;
}

inline auto MakeSignalConfig(std::string_view protocol,
                             std::string_view endpointHttp,
                             std::string_view endpointGrpc,
                             std::string_view headers,
                             uint32_t otlpHttpJson) -> nestdaq_otel_signal_config
{
    auto config = nestdaq_otel_signal_config{};
    config.protocol = protocol.data();
    config.endpoint_http = endpointHttp.data();
    config.endpoint_grpc = endpointGrpc.data();
    config.headers = headers.data();
    config.otlp_http_json = otlpHttpJson;
    return config;
}

inline auto ParseBool(std::string_view value) -> bool
{
    return value == "1" || value == "true" || value == "TRUE" ||
           value == "on" || value == "ON" || value == "yes" || value == "YES";
}

inline auto ParseTelemetryOptions(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                                  std::string_view defaultServiceName) -> TelemetryOptions
{
    auto options = TelemetryOptions{};
    options.serviceName = defaultServiceName;
    if (argv == nullptr) {
        ApplyEnvironment(options);
        EnsureServiceInstanceId(options);
        return options;
    }
    if (argc > 0) {
        if (const auto executable = Basename(argv[0]); !executable.empty()) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            options.serviceName = executable;
        }
    }
    ApplyEnvironment(options);
    auto explicitTelemetryServiceName = false;
    auto explicitTelemetryServiceInstanceId = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]}; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg.rfind("--", 0) != 0) {
            continue;
        }

        auto key = arg.substr(2);
        std::string_view value;
        const auto equals = key.find('=');
        if (equals != std::string_view::npos) {
            value = key.substr(equals + 1);
            key = key.substr(0, equals);
        } else if ((key.rfind("otel-", 0) == 0 || key == "service-name" || key == "uuid") && i + 1 < argc &&
                   std::string_view{argv[i + 1]}.rfind("--", 0) != 0) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            value = argv[++i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        } else if (key == "otel-log-protocol" || key == "otel-metric-protocol" || key == "otel-trace-protocol") {
            value = "";
        } else {
            continue;
        }
        if (key == "otel-service-name") {
            AssignOption(options, key, value);
            explicitTelemetryServiceName = true;
        } else if (key == "service-name") {
            if (!explicitTelemetryServiceName) {
                options.serviceName = value;
            }
        } else if (key == "otel-service-instance-id") {
            AssignOption(options, key, value);
            options.generatedServiceInstanceId = false;
            explicitTelemetryServiceInstanceId = true;
        } else if (key == "uuid") {
            if (!explicitTelemetryServiceInstanceId) {
                options.serviceInstanceId = value;
                options.generatedServiceInstanceId = false;
            }
        } else {
            AssignOption(options, key, value);
        }
    }

    EnsureServiceInstanceId(options);
    return options;
}

inline auto ParseFairLoggerSeverity(std::string_view severity) -> SeverityParseResult
{
    if (const auto it = fair::Logger::fSeverityMap.find(severity);
        it != fair::Logger::fSeverityMap.end()) {
        return SeverityParseResult{
            .value = static_cast<int32_t>(it->second),
            .usedFallback = false,
        };
    }
    return SeverityParseResult{
        .value = static_cast<int32_t>(fair::Severity::info),
        .usedFallback = true,
    };
}

inline auto ParseUInt32(std::string_view value, uint32_t fallback) -> uint32_t
{
    try {
        return static_cast<uint32_t>(std::stoul(std::string{value}));
    } catch (...) {
        return fallback;
    }
}

inline auto ReadTelemetryOptions(const boost::program_options::variables_map& vm,
                                 std::string_view defaultServiceName) -> TelemetryOptions
{
    auto options = TelemetryOptions{};
    options.serviceName = defaultServiceName;
    ApplyEnvironment(options);

    const auto readString = [&vm, &options](std::string_view key) {
        const auto name = std::string{key};
        if (vm.count(name) != 0) {
            AssignOption(options, key, vm[name].as<std::string>());
        }
    };

    readString("otel-library");
    readString("otel-log-protocol");
    readString("otel-metric-protocol");
    readString("otel-trace-protocol");
    readString("otel-log-endpoint-http");
    readString("otel-log-endpoint-grpc");
    readString("otel-metric-endpoint-http");
    readString("otel-metric-endpoint-grpc");
    readString("otel-trace-endpoint-http");
    readString("otel-trace-endpoint-grpc");
    readString("otel-log-headers");
    readString("otel-metric-headers");
    readString("otel-trace-headers");
    readString("otel-log-severity");
    readString("otel-service-name");
    readString("otel-service-namespace");
    readString("otel-service-instance-id");
    readString("otel-fairmq-id");
    readString("otel-fairmq-device");
    readString("otel-fairmq-session");
    readString("otel-fairmq-transport");

    if (vm.count("otel-log-required") != 0) {
        options.required = vm["otel-log-required"].as<bool>();
    }
    if (vm.count("otel-timeout-ms") != 0) {
        options.timeoutMs = vm["otel-timeout-ms"].as<uint32_t>();
    }
    if (vm.count("otel-metric-export-interval-ms") != 0) {
        options.metricExportIntervalMs = vm["otel-metric-export-interval-ms"].as<uint32_t>();
    }
    if (vm.count("otel-log-http-json") != 0) {
        options.logOtlpHttpJson = vm["otel-log-http-json"].as<bool>() ? 1U : 0U;
    }
    if (vm.count("otel-metric-http-json") != 0) {
        options.metricOtlpHttpJson = vm["otel-metric-http-json"].as<bool>() ? 1U : 0U;
    }
    if (vm.count("otel-trace-http-json") != 0) {
        options.traceOtlpHttpJson = vm["otel-trace-http-json"].as<bool>() ? 1U : 0U;
    }
    EnsureServiceInstanceId(options);
    return options;
}

inline auto WarnUnknownSeverityFallback(std::string_view severity) -> void
{
    if (!ParseFairLoggerSeverity(severity).usedFallback) {
        return;
    }
    LOG(warn) << "Unknown otel-log-severity '" << severity << "', using FairLogger severity '"
              << fair::Logger::SeverityName(fair::Severity::info) << "'";
}

inline auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t
{
    return ParseFairLoggerSeverity(severity).value;
}

inline auto SetGeneratedUuidProperty(fair::mq::ProgOptions& config,
                                     const TelemetryOptions& options,
                                     std::string_view key) -> void
{
    if (!options.generatedServiceInstanceId || options.serviceInstanceId.empty()) {
        return;
    }
    const auto propertyKey = std::string{key};
    if (config.Count(propertyKey) != 0) {
        return;
    }
    config.SetProperty<std::string>(propertyKey, options.serviceInstanceId);
}

/**
 * @brief Runtime loader for the optional OpenTelemetry plugin.
 *
 * `TelemetryLibrary` owns the `dlopen()` handle and resolves the required
 * `nestdaq_otel_*` C ABI symbols. It is intentionally non-copyable and
 * non-movable because the plugin state is process-wide. Destruction shuts down
 * telemetry once and then closes the shared library.
 */
class TelemetryLibrary {
public:
    TelemetryLibrary() = default;
    TelemetryLibrary(const TelemetryLibrary&) = delete;
    TelemetryLibrary& operator=(const TelemetryLibrary&) = delete;
    TelemetryLibrary(TelemetryLibrary&&) = delete;
    TelemetryLibrary& operator=(TelemetryLibrary&&) = delete;

    ~TelemetryLibrary()
    {
        ShutdownTelemetry(kDefaultTimeoutMs);
        if (fHandle) {
            dlclose(fHandle);
        }
    }

    /** Return the last loader or plugin error captured by this wrapper. */
    auto GetLastError() const -> const std::string&
    {
        return fLastError;
    }

    /** Initialize the loaded plugin with a C ABI configuration. */
    auto InitializeWith(const nestdaq_otel_config& config) -> bool
    {
        if (!fInitialize) {
            return false;
        }
        const auto rc = fInitialize(&config);
        if (rc != 0) {
            if (fLastErrorFunction) {
                if (const auto* error = fLastErrorFunction()) {
                    fLastError = error;
                }
            }
            return false;
        }
        return true;
    }

    /** Forward a double counter measurement through the loaded plugin. */
    auto MetricAddDoubleCounter(std::string_view name,
                                double value,
                                std::string_view unit = "",
                                std::string_view description = "",
                                const nestdaq_otel_attribute* attributes = nullptr,
                                uint64_t attributeCount = 0) -> bool
    {
        if (!fMetricAddDoubleCounter) {
            return false;
        }
        return StoreResult(fMetricAddDoubleCounter(name.data(),
                                                   value,
                                                   unit.data(),
                                                   description.data(),
                                                   attributes,
                                                   attributeCount));
    }

    /** Forward a double histogram measurement through the loaded plugin. */
    auto MetricRecordDoubleHistogram(std::string_view name,
                                     double value,
                                     std::string_view unit = "",
                                     std::string_view description = "",
                                     const nestdaq_otel_attribute* attributes = nullptr,
                                     uint64_t attributeCount = 0) -> bool
    {
        if (!fMetricRecordDoubleHistogram) {
            return false;
        }
        return StoreResult(fMetricRecordDoubleHistogram(name.data(),
                                                        value,
                                                        unit.data(),
                                                        description.data(),
                                                        attributes,
                                                        attributeCount));
    }

    /**
     * @brief Load the telemetry shared library and resolve ABI symbols.
     *
     * @param library Path or soname passed to `dlopen()`.
     */
    auto Load(const std::string& library) -> bool
    {
        fHandle = dlopen(library.data(), RTLD_NOW | RTLD_LOCAL);
        if (!fHandle) {
            fLastError = dlerror(); // NOLINT(concurrency-mt-unsafe)
            return false;
        }

        fInitialize = Resolve<int (*)(const nestdaq_otel_config*)>("nestdaq_otel_init");
        fShutdown = Resolve<int (*)(uint64_t)>("nestdaq_otel_shutdown");
        fLastErrorFunction = Resolve<const char* (*)()>("nestdaq_otel_last_error");
        fSetMinSeverity = Resolve<int (*)(int32_t)>("nestdaq_otel_set_min_severity");
        fMetricAddDoubleCounter = Resolve<int (*)(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
            "nestdaq_otel_metric_add_double_counter");
        fMetricRecordDoubleHistogram = Resolve<int (*)(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
            "nestdaq_otel_metric_record_double_histogram");
        fSpanEnd = Resolve<int (*)(uint64_t)>("nestdaq_otel_span_end");
        fSpanSetAttribute = Resolve<int (*)(uint64_t, const nestdaq_otel_attribute*)>("nestdaq_otel_span_set_attribute");
        fSpanStart = Resolve<uint64_t (*)(const char*, const nestdaq_otel_attribute*, uint64_t)>("nestdaq_otel_span_start");

        if (!fInitialize || !fShutdown) {
            fLastError = "telemetry library does not export the required nestdaq_otel_* C ABI";
            dlclose(fHandle);
            fHandle = nullptr;
            fInitialize = nullptr;
            fShutdown = nullptr;
            fLastErrorFunction = nullptr;
            fSetMinSeverity = nullptr;
            fMetricAddDoubleCounter = nullptr;
            fMetricRecordDoubleHistogram = nullptr;
            fSpanEnd = nullptr;
            fSpanSetAttribute = nullptr;
            fSpanStart = nullptr;
            return false;
        }

        fLastError.clear();
        return true;
    }

    /** End an active span handle. */
    auto SpanEnd(uint64_t spanHandle) -> bool
    {
        if (!fSpanEnd) {
            return false;
        }
        return StoreResult(fSpanEnd(spanHandle));
    }

    /** Set an attribute on an active span handle. */
    auto SpanSetAttribute(uint64_t spanHandle, const nestdaq_otel_attribute& attribute) -> bool
    {
        if (!fSpanSetAttribute) {
            return false;
        }
        return StoreResult(fSpanSetAttribute(spanHandle, &attribute));
    }

    /** Start a span and return the plugin-owned span handle. */
    auto SpanStart(std::string_view name,
                   const nestdaq_otel_attribute* attributes = nullptr,
                   uint64_t attributeCount = 0) -> uint64_t
    {
        if (!fSpanStart) {
            return 0;
        }
        const auto spanHandle = fSpanStart(name.data(), attributes, attributeCount);
        if (spanHandle == 0) {
            StoreResult(NESTDAQ_OTEL_ERROR);
        } else {
            fLastError.clear();
        }
        return spanHandle;
    }

    /** Update the FairLogger severity threshold by name. */
    auto SetMinSeverity(std::string_view severity) -> bool
    {
        const auto parsedSeverity = ParseFairLoggerSeverity(severity);
        const auto updated = SetMinSeverity(parsedSeverity.value);
        if (updated && parsedSeverity.usedFallback) {
            WarnUnknownSeverityFallback(severity);
        }
        return updated;
    }

    /** Update the FairLogger severity threshold by numeric FairLogger value. */
    auto SetMinSeverity(int32_t severity) -> bool
    {
        if (!fSetMinSeverity) {
            return false;
        }
        const auto rc = fSetMinSeverity(severity);
        if (rc != 0) {
            if (fLastErrorFunction) {
                if (const auto* error = fLastErrorFunction()) {
                    fLastError = error;
                }
            }
            return false;
        }
        fLastError.clear();
        return true;
    }

    /** Shut down telemetry if it has not already been shut down. */
    auto ShutdownTelemetry(uint64_t timeoutMs) const -> void
    {
        if (fShutdown && !fShutdownCalled) {
            fShutdownCalled = true;
            fShutdown(timeoutMs);
        }
    }

private:
    template<typename T>
    auto Resolve(const char* symbol) const -> T
    {
        dlerror(); // NOLINT(concurrency-mt-unsafe)
        return reinterpret_cast<T>(dlsym(fHandle, symbol)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }

    auto StoreResult(int rc) -> bool
    {
        if (rc == 0) {
            fLastError.clear();
            return true;
        }
        if (fLastErrorFunction) {
            if (const auto* error = fLastErrorFunction()) {
                fLastError = error;
            }
        }
        return false;
    }

    void* fHandle{nullptr};
    std::function<int(const nestdaq_otel_config*)> fInitialize;
    std::function<int(uint64_t)> fShutdown;
    std::function<const char*()> fLastErrorFunction;
    std::function<int(int32_t)> fSetMinSeverity;
    std::function<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)> fMetricAddDoubleCounter;
    std::function<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)> fMetricRecordDoubleHistogram;
    std::function<int(uint64_t)> fSpanEnd;
    std::function<int(uint64_t, const nestdaq_otel_attribute*)> fSpanSetAttribute;
    std::function<uint64_t(const char*, const nestdaq_otel_attribute*, uint64_t)> fSpanStart;
    mutable bool fShutdownCalled{false};
    std::string fLastError;
};

inline auto SubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config,
                                            TelemetryLibrary& telemetry) -> void
{
    config.SubscribeAsString(std::string{kTelemetryConfigSubscriber},
                             [&telemetry](const fair::mq::PropertyChange::KeyType& key, std::string value) {
                                 if (key != "otel-log-severity") {
                                     return;
                                 }
                                 if (!telemetry.SetMinSeverity(value)) {
                                     LOG(error) << "Failed to update OTel log severity: "
                                                << telemetry.GetLastError();
                                 }
                             });
}

inline auto UnsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void
{
    config.UnsubscribeAsString(std::string{kTelemetryConfigSubscriber});
}

} // namespace nestdaq::telemetry
