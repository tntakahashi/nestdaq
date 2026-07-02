/** @file
 *  @brief Implements telemetry option parsing and the runtime OpenTelemetry plugin loader.
 */

#include "nestdaq/telemetry/FairLoggerTelemetryLoader.h"

#include <fairmq/ProgOptions.h>
#include <fairmq/Version.h>
#include <fairlogger/Logger.h>

#include <boost/program_options.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <algorithm>
#include <array>
#include <dlfcn.h>

#include <cstdlib>
#include <unistd.h>

namespace nestdaq::telemetry {
namespace {
constexpr auto kHostNameBufferSize = std::size_t{256};

/**
 * @brief Resolve one symbol from the loaded telemetry plugin.
 *
 * The result is wrapped in `std::function` so callers can store optional C ABI
 * entries uniformly and test whether a symbol was present before calling it.
 */
template<typename T>
auto ResolveSymbol(void* handle, const char* symbol) -> std::function<T> {
    dlerror(); // NOLINT(concurrency-mt-unsafe)
    return reinterpret_cast<T*>(dlsym(handle, symbol)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

auto IsValidSpdlogAsyncOverflowPolicy(std::string_view value) -> bool {
    return value == "block" || value == "overrun_oldest" || value == "discard_new";
}

auto NormalizeSpdlogAsyncOptions(TelemetryOptions& options) -> void {
    if (options.spdlogAsyncQueueSize == 0) {
        options.spdlogAsyncQueueSize = kDefaultSpdlogAsyncQueueSize;
    }
    if (options.spdlogAsyncThreadCount == 0) {
        options.spdlogAsyncThreadCount = kDefaultSpdlogAsyncThreadCount;
    }
    if (!IsValidSpdlogAsyncOverflowPolicy(options.spdlogAsyncOverflowPolicy)) {
        options.spdlogAsyncOverflowPolicy = kDefaultSpdlogAsyncOverflowPolicy;
    }
}

} // namespace

auto AddTelemetryOptions(boost::program_options::options_description& options,
                         std::string_view defaultServiceName) -> void {
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
           ("otel-fairmq-transport", bpo::value<std::string>(), "FairMQ transport resource attribute")
           ("spdlog-console-pattern", bpo::value<std::string>()->default_value(std::string{kDefaultSpdlogConsolePattern}), "spdlog native console sink pattern")
           ("spdlog-native-console", bpo::value<bool>()->default_value(true), "Enable spdlog native console sink independently from OTel spdlog sink")
           ("spdlog-async", bpo::value<bool>()->default_value(false), "Use spdlog async logger for NestDAQ helper loggers")
           ("spdlog-async-queue-size", bpo::value<uint32_t>()->default_value(kDefaultSpdlogAsyncQueueSize), "spdlog async queue size")
           ("spdlog-async-thread-count", bpo::value<uint32_t>()->default_value(kDefaultSpdlogAsyncThreadCount), "spdlog async worker thread count")
           ("spdlog-async-overflow-policy", bpo::value<std::string>()->default_value(std::string{kDefaultSpdlogAsyncOverflowPolicy}), "spdlog async overflow policy: block, overrun_oldest, discard_new");
}

auto ApplyEnvironment(TelemetryOptions& options) -> void {
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
    if (const auto* value = Env("NESTDAQ_SPDLOG_CONSOLE_PATTERN")) {
        options.spdlogConsolePattern = value;
    }
    if (const auto* value = Env("NESTDAQ_SPDLOG_NATIVE_CONSOLE")) {
        options.spdlogNativeConsole = ParseBool(value);
    }
    if (const auto* value = Env("NESTDAQ_SPDLOG_ASYNC")) {
        options.spdlogAsync = ParseBool(value);
    }
    if (const auto* value = Env("NESTDAQ_SPDLOG_ASYNC_QUEUE_SIZE")) {
        options.spdlogAsyncQueueSize = ParseUInt32(value, options.spdlogAsyncQueueSize);
    }
    if (const auto* value = Env("NESTDAQ_SPDLOG_ASYNC_THREAD_COUNT")) {
        options.spdlogAsyncThreadCount = ParseUInt32(value, options.spdlogAsyncThreadCount);
    }
    if (const auto* value = Env("NESTDAQ_SPDLOG_ASYNC_OVERFLOW_POLICY")) {
        options.spdlogAsyncOverflowPolicy = value;
    }
}

auto AssignOption(TelemetryOptions& options, std::string_view key, std::string_view value) -> void {
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
    } else if (key == "spdlog-console-pattern") {
        options.spdlogConsolePattern = value;
    } else if (key == "spdlog-native-console") {
        options.spdlogNativeConsole = ParseBool(value);
    } else if (key == "spdlog-async") {
        options.spdlogAsync = ParseBool(value);
    } else if (key == "spdlog-async-queue-size") {
        options.spdlogAsyncQueueSize = ParseUInt32(value, options.spdlogAsyncQueueSize);
    } else if (key == "spdlog-async-thread-count") {
        options.spdlogAsyncThreadCount = ParseUInt32(value, options.spdlogAsyncThreadCount);
    } else if (key == "spdlog-async-overflow-policy") {
        options.spdlogAsyncOverflowPolicy = value;
    }
}

auto Basename(std::string_view path) -> std::string_view {
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string_view::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

auto Env(const char* name) -> const char* {
    return std::getenv(name); // NOLINT(concurrency-mt-unsafe)
}

auto DetectHostName() -> std::string {
    auto buffer = std::array<char, kHostNameBufferSize> {};
    if (gethostname(buffer.data(), buffer.size()) != 0) {
        return {};
    }
    if (buffer.back() != '\0') {
        return {};
    }
    return std::string{buffer.data()};
}

auto EnsureHostName(TelemetryOptions& options) -> void {
    if (!options.hostName.empty()) {
        return;
    }
    options.hostName = DetectHostName();
}

auto EnsureServiceInstanceId(TelemetryOptions& options) -> void {
    if (!options.serviceInstanceId.empty()) {
        return;
    }
    options.serviceInstanceId = GenerateUuidString();
    options.generatedServiceInstanceId = true;
}

auto GenerateUuidString() -> std::string {
    return boost::uuids::to_string(boost::uuids::random_generator{}());
}

auto ToLowerAscii(std::string_view value) -> std::string {
    auto lowered = std::string{value};
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return lowered;
}

auto NormalizeServiceName(TelemetryOptions& options) -> void {
    options.serviceName = ToLowerAscii(options.serviceName);
}

auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config {
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
    config.host_name = options.hostName.data();
    config.nestdaq_instance_id = options.nestdaqInstanceId.data();
    config.nestdaq_instance_id_status = options.nestdaqInstanceIdStatus.data();
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

auto MakeSignalConfig(std::string_view protocol,
                      std::string_view endpointHttp,
                      std::string_view endpointGrpc,
                      std::string_view headers,
                      uint32_t otlpHttpJson) -> nestdaq_otel_signal_config {
    auto config = nestdaq_otel_signal_config{};
    config.protocol = protocol.data();
    config.endpoint_http = endpointHttp.data();
    config.endpoint_grpc = endpointGrpc.data();
    config.headers = headers.data();
    config.otlp_http_json = otlpHttpJson;
    return config;
}

auto ParseBool(std::string_view value) -> bool {
    return value == "1" || value == "true" || value == "TRUE" ||
           value == "on" || value == "ON" || value == "yes" || value == "YES";
}

auto ParseTelemetryOptions(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                           std::string_view defaultServiceName) -> TelemetryOptions {
    auto options = TelemetryOptions{};
    options.serviceName = defaultServiceName;
    if (argv == nullptr) {
        ApplyEnvironment(options);
        NormalizeSpdlogAsyncOptions(options);
        EnsureHostName(options);
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
        } else if ((key.rfind("otel-", 0) == 0 || key.rfind("spdlog-", 0) == 0 || key == "service-name" || key == "uuid") && i + 1 < argc &&
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

    NormalizeServiceName(options);
    NormalizeSpdlogAsyncOptions(options);
    EnsureHostName(options);
    EnsureServiceInstanceId(options);
    return options;
}

auto ParseFairLoggerSeverity(std::string_view severity) -> SeverityParseResult {
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

auto ParseUInt32(std::string_view value, uint32_t fallback) -> uint32_t {
    try {
        return static_cast<uint32_t>(std::stoul(std::string{value}));
    } catch (...) {
        return fallback;
    }
}

auto ReadTelemetryOptions(const boost::program_options::variables_map& vm,
                          std::string_view defaultServiceName) -> TelemetryOptions {
    auto options = TelemetryOptions{};
    options.serviceName = defaultServiceName;
    ApplyEnvironment(options);

    const auto readString = [&vm, &options](std::string_view key) {
        const auto name = std::string{key};
        if (vm.count(name) != 0 && !vm[name].defaulted()) {
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
    readString("spdlog-console-pattern");

    if (vm.count("spdlog-native-console") != 0 && !vm["spdlog-native-console"].defaulted()) {
        options.spdlogNativeConsole = vm["spdlog-native-console"].as<bool>();
    }
    if (vm.count("spdlog-async") != 0 && !vm["spdlog-async"].defaulted()) {
        options.spdlogAsync = vm["spdlog-async"].as<bool>();
    }
    if (vm.count("spdlog-async-queue-size") != 0 && !vm["spdlog-async-queue-size"].defaulted()) {
        options.spdlogAsyncQueueSize = vm["spdlog-async-queue-size"].as<uint32_t>();
    }
    if (vm.count("spdlog-async-thread-count") != 0 && !vm["spdlog-async-thread-count"].defaulted()) {
        options.spdlogAsyncThreadCount = vm["spdlog-async-thread-count"].as<uint32_t>();
    }
    readString("spdlog-async-overflow-policy");
    if (vm.count("otel-log-required") != 0 && !vm["otel-log-required"].defaulted()) {
        options.required = vm["otel-log-required"].as<bool>();
    }
    if (vm.count("otel-timeout-ms") != 0 && !vm["otel-timeout-ms"].defaulted()) {
        options.timeoutMs = vm["otel-timeout-ms"].as<uint32_t>();
    }
    if (vm.count("otel-metric-export-interval-ms") != 0 && !vm["otel-metric-export-interval-ms"].defaulted()) {
        options.metricExportIntervalMs = vm["otel-metric-export-interval-ms"].as<uint32_t>();
    }
    if (vm.count("otel-log-http-json") != 0 && !vm["otel-log-http-json"].defaulted()) {
        options.logOtlpHttpJson = vm["otel-log-http-json"].as<bool>() ? 1U : 0U;
    }
    if (vm.count("otel-metric-http-json") != 0 && !vm["otel-metric-http-json"].defaulted()) {
        options.metricOtlpHttpJson = vm["otel-metric-http-json"].as<bool>() ? 1U : 0U;
    }
    if (vm.count("otel-trace-http-json") != 0 && !vm["otel-trace-http-json"].defaulted()) {
        options.traceOtlpHttpJson = vm["otel-trace-http-json"].as<bool>() ? 1U : 0U;
    }
    NormalizeServiceName(options);
    NormalizeSpdlogAsyncOptions(options);
    EnsureHostName(options);
    EnsureServiceInstanceId(options);
    return options;
}

auto WarnUnknownSeverityFallback(std::string_view severity) -> void {
    if (!ParseFairLoggerSeverity(severity).usedFallback) {
        return;
    }
    LOG(warn) << "Unknown otel-log-severity '" << severity << "', using FairLogger severity '"
              << fair::Logger::SeverityName(fair::Severity::info) << "'";
}

auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t {
    return ParseFairLoggerSeverity(severity).value;
}

auto SetGeneratedUuidProperty(fair::mq::ProgOptions& config,
                              const TelemetryOptions& options,
                              std::string_view key) -> void {
    if (!options.generatedServiceInstanceId || options.serviceInstanceId.empty()) {
        return;
    }
    const auto propertyKey = std::string{key};
    if (config.Count(propertyKey) != 0) {
        return;
    }
    config.SetProperty<std::string>(propertyKey, options.serviceInstanceId);
}

TelemetryLibrary::~TelemetryLibrary() {
    ShutdownTelemetry(kDefaultTimeoutMs);
    if (fHandle) {
        dlclose(fHandle);
    }
}

auto TelemetryLibrary::GetLastError() const -> const std::string& {
    return fLastError;
}

auto TelemetryLibrary::InitializeWith(const nestdaq_otel_config& config) -> bool {
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
    fShutdownCalled = false;
    fLogExportEnabled = config.logs.protocol != nullptr && !std::string_view{config.logs.protocol}.empty();
    fLastError.clear();
    return true;
}

auto TelemetryLibrary::ForceFlush(uint64_t timeoutMs) -> bool {
    if (!fForceFlush) {
        return false;
    }
    return StoreResult(fForceFlush(timeoutMs));
}

auto TelemetryLibrary::CreateSpdlogSink() const -> std::shared_ptr<spdlog::sinks::sink> {
    if (!fLogExportEnabled || !fCreateSpdlogSink) {
        return {};
    }
    return fCreateSpdlogSink();
}

auto TelemetryLibrary::RecordFrameworkFairMQState(int64_t stateId, std::string_view stateName) -> void {
    if (!fRecordFrameworkFairMQState) {
        return;
    }
    const auto value = std::string{stateName};
    fRecordFrameworkFairMQState(stateId, value.data());
}

auto TelemetryLibrary::MetricAddDoubleCounter(std::string_view name,
        double value,
        std::string_view unit,
        std::string_view description,
        const nestdaq_otel_attribute* attributes,
        uint64_t attributeCount) -> bool {
    if (!fMetricAddDoubleCounter) {
        return false;
    }
    return StoreResult(fMetricAddDoubleCounter(name.data(), value, unit.data(), description.data(), attributes, attributeCount));
}

auto TelemetryLibrary::MetricRecordDoubleHistogram(std::string_view name,
        double value,
        std::string_view unit,
        std::string_view description,
        const nestdaq_otel_attribute* attributes,
        uint64_t attributeCount) -> bool {
    if (!fMetricRecordDoubleHistogram) {
        return false;
    }
    return StoreResult(fMetricRecordDoubleHistogram(name.data(), value, unit.data(), description.data(), attributes, attributeCount));
}

auto TelemetryLibrary::MetricRecordDoubleGauge(std::string_view name,
        double value,
        std::string_view unit,
        std::string_view description,
        const nestdaq_otel_attribute* attributes,
        uint64_t attributeCount) -> bool {
    if (!fMetricRecordDoubleGauge) {
        return false;
    }
    return StoreResult(fMetricRecordDoubleGauge(name.data(), value, unit.data(), description.data(), attributes, attributeCount));
}

auto TelemetryLibrary::Load(const std::string& library) -> bool {
    auto flags = RTLD_NOW | RTLD_LOCAL;
#ifdef RTLD_NODELETE
    // OpenTelemetry providers are process-wide; avoid unmapping plugin code
    // while SDK state or background shutdown paths may still reference it.
    flags |= RTLD_NODELETE;
#endif
    fHandle = dlopen(library.data(), flags);
    if (!fHandle) {
        fLastError = dlerror(); // NOLINT(concurrency-mt-unsafe)
        return false;
    }

    fInitialize = ResolveSymbol<int(const nestdaq_otel_config*)>(fHandle, "nestdaq_otel_init");
    fForceFlush = ResolveSymbol<int(uint64_t)>(fHandle, "nestdaq_otel_force_flush");
    fCreateSpdlogSink = ResolveSymbol<std::shared_ptr<spdlog::sinks::sink>()>(fHandle, "nestdaq_otel_create_spdlog_sink");
    fShutdown = ResolveSymbol<int(uint64_t)>(fHandle, "nestdaq_otel_shutdown");
    fLastErrorFunction = ResolveSymbol<const char*()>(fHandle, "nestdaq_otel_last_error");
    fRecordFrameworkFairMQState = ResolveSymbol<void(int64_t, const char*)>(
                                      fHandle, "nestdaq_otel_framework_record_fairmq_state");
    fSetMinSeverity = ResolveSymbol<int(int32_t)>(fHandle, "nestdaq_otel_set_min_severity");
    fSetNestdaqInstanceId = ResolveSymbol<int(const char*)>(fHandle, "nestdaq_otel_set_nestdaq_instance_id");
    fMetricAddDoubleCounter = ResolveSymbol<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
                                  fHandle, "nestdaq_otel_metric_add_double_counter");
    fMetricRecordDoubleHistogram = ResolveSymbol<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
                                       fHandle, "nestdaq_otel_metric_record_double_histogram");
    fMetricRecordDoubleGauge = ResolveSymbol<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
                                   fHandle, "nestdaq_otel_metric_record_double_gauge");
    fSpanEnd = ResolveSymbol<int(uint64_t)>(fHandle, "nestdaq_otel_span_end");
    fSpanSetAttribute = ResolveSymbol<int(uint64_t, const nestdaq_otel_attribute*)>(fHandle, "nestdaq_otel_span_set_attribute");
    fSpanStart = ResolveSymbol<uint64_t(const char*, const nestdaq_otel_attribute*, uint64_t)>(fHandle, "nestdaq_otel_span_start");

    if (!fInitialize || !fShutdown) {
        fLastError = "telemetry library does not export the required nestdaq_otel_* C ABI";
        dlclose(fHandle);
        fHandle = nullptr;
        fInitialize = nullptr;
        fForceFlush = nullptr;
        fCreateSpdlogSink = nullptr;
        fShutdown = nullptr;
        fLastErrorFunction = nullptr;
        fRecordFrameworkFairMQState = nullptr;
        fSetMinSeverity = nullptr;
        fSetNestdaqInstanceId = nullptr;
        fMetricAddDoubleCounter = nullptr;
        fMetricRecordDoubleHistogram = nullptr;
        fMetricRecordDoubleGauge = nullptr;
        fSpanEnd = nullptr;
        fSpanSetAttribute = nullptr;
        fSpanStart = nullptr;
        return false;
    }

    fLastError.clear();
    return true;
}

auto TelemetryLibrary::SpanEnd(uint64_t spanHandle) -> bool {
    if (!fSpanEnd) {
        return false;
    }
    return StoreResult(fSpanEnd(spanHandle));
}

auto TelemetryLibrary::SpanSetAttribute(uint64_t spanHandle, const nestdaq_otel_attribute& attribute) -> bool {
    if (!fSpanSetAttribute) {
        return false;
    }
    return StoreResult(fSpanSetAttribute(spanHandle, &attribute));
}

auto TelemetryLibrary::SetNestdaqInstanceId(std::string_view instanceId) -> bool {
    if (!fSetNestdaqInstanceId) {
        return false;
    }
    const auto value = std::string{instanceId};
    return StoreResult(fSetNestdaqInstanceId(value.data()));
}

auto TelemetryLibrary::SpanStart(std::string_view name,
                                 const nestdaq_otel_attribute* attributes,
                                 uint64_t attributeCount) -> uint64_t {
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

auto TelemetryLibrary::SetMinSeverity(std::string_view severity) -> bool {
    const auto parsedSeverity = ParseFairLoggerSeverity(severity);
    const auto updated = SetMinSeverity(parsedSeverity.value);
    if (updated && parsedSeverity.usedFallback) {
        WarnUnknownSeverityFallback(severity);
    }
    return updated;
}

auto TelemetryLibrary::SetMinSeverity(int32_t severity) -> bool {
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

auto TelemetryLibrary::ShutdownTelemetry(uint64_t timeoutMs) const -> void {
    fLogExportEnabled = false;
    if (fShutdown && !fShutdownCalled) {
        fShutdownCalled = true;
        fShutdown(timeoutMs);
    }
}

auto TelemetryLibrary::StoreResult(int rc) -> bool {
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

auto SubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config,
                                     TelemetryLibrary& telemetry) -> void {
    config.SubscribeAsString(std::string{kTelemetryConfigSubscriber},
    [&telemetry](const fair::mq::PropertyChange::KeyType& key, std::string value) {
        if (key == "id") {
            if (!telemetry.SetNestdaqInstanceId(value)) {
                LOG(error) << "Failed to update OTel NestDAQ instance id: "
                           << telemetry.GetLastError();
            }
            return;
        }
        if (key == "otel-log-severity" && !telemetry.SetMinSeverity(value)) {
            LOG(error) << "Failed to update OTel log severity: "
                       << telemetry.GetLastError();
        }
    });
}

auto UnsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void {
    config.UnsubscribeAsString(std::string{kTelemetryConfigSubscriber});
}

} // namespace nestdaq::telemetry
