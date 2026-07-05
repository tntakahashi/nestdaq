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
auto resolveSymbol(void* handle, const char* symbol) -> std::function<T> {
    dlerror(); // NOLINT(concurrency-mt-unsafe)
    return reinterpret_cast<T*>(dlsym(handle, symbol)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

auto isValidSpdlogAsyncOverflowPolicy(std::string_view value) -> bool {
    return value == "block" || value == "overrun_oldest" || value == "discard_new";
}

auto normalizeSpdlogAsyncOptions(TelemetryOptions& options) -> void {
    if (options.spdlogAsyncQueueSize == 0) {
        options.spdlogAsyncQueueSize = kDefaultSpdlogAsyncQueueSize;
    }
    if (options.spdlogAsyncThreadCount == 0) {
        options.spdlogAsyncThreadCount = kDefaultSpdlogAsyncThreadCount;
    }
    if (!isValidSpdlogAsyncOverflowPolicy(options.spdlogAsyncOverflowPolicy)) {
        options.spdlogAsyncOverflowPolicy = kDefaultSpdlogAsyncOverflowPolicy;
    }
}

} // namespace

auto addTelemetryOptions(boost::program_options::options_description& options,
                         std::string_view default_service_name) -> void {
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
           ("otel-service-name", bpo::value<std::string>()->default_value(std::string{default_service_name}), "OTel service.name resource attribute")
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

auto applyEnvironment(TelemetryOptions& options) -> void {
    if (const auto* value = env("NESTDAQ_OTEL_LIBRARY")) {
        options.library = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_LOG_PROTOCOL")) {
        options.logProtocol = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_METRIC_PROTOCOL")) {
        options.metricProtocol = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_TRACE_PROTOCOL")) {
        options.traceProtocol = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_LOG_ENDPOINT_HTTP")) {
        options.logEndpointHttp = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_LOG_ENDPOINT_GRPC")) {
        options.logEndpointGrpc = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_METRIC_ENDPOINT_HTTP")) {
        options.metricEndpointHttp = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_METRIC_ENDPOINT_GRPC")) {
        options.metricEndpointGrpc = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_TRACE_ENDPOINT_HTTP")) {
        options.traceEndpointHttp = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_TRACE_ENDPOINT_GRPC")) {
        options.traceEndpointGrpc = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_LOG_HEADERS")) {
        options.logHeaders = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_METRIC_HEADERS")) {
        options.metricHeaders = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_TRACE_HEADERS")) {
        options.traceHeaders = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_LOG_SEVERITY")) {
        options.severity = value;
    }
    if (const auto* value = env("NESTDAQ_OTEL_LOG_REQUIRED")) {
        options.required = parseBool(value);
    }
    if (const auto* value = env("NESTDAQ_SPDLOG_CONSOLE_PATTERN")) {
        options.spdlogConsolePattern = value;
    }
    if (const auto* value = env("NESTDAQ_SPDLOG_NATIVE_CONSOLE")) {
        options.spdlogNativeConsole = parseBool(value);
    }
    if (const auto* value = env("NESTDAQ_SPDLOG_ASYNC")) {
        options.spdlogAsync = parseBool(value);
    }
    if (const auto* value = env("NESTDAQ_SPDLOG_ASYNC_QUEUE_SIZE")) {
        options.spdlogAsyncQueueSize = parseUInt32(value, options.spdlogAsyncQueueSize);
    }
    if (const auto* value = env("NESTDAQ_SPDLOG_ASYNC_THREAD_COUNT")) {
        options.spdlogAsyncThreadCount = parseUInt32(value, options.spdlogAsyncThreadCount);
    }
    if (const auto* value = env("NESTDAQ_SPDLOG_ASYNC_OVERFLOW_POLICY")) {
        options.spdlogAsyncOverflowPolicy = value;
    }
}

auto assignOption(TelemetryOptions& options, std::string_view key, std::string_view value) -> void {
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
        options.required = parseBool(value);
    } else if (key == "otel-timeout-ms") {
        options.timeoutMs = parseUInt32(value, options.timeoutMs);
    } else if (key == "otel-metric-export-interval-ms") {
        options.metricExportIntervalMs = parseUInt32(value, options.metricExportIntervalMs);
    } else if (key == "otel-log-http-json") {
        options.logOtlpHttpJson = parseBool(value) ? 1U : 0U;
    } else if (key == "otel-metric-http-json") {
        options.metricOtlpHttpJson = parseBool(value) ? 1U : 0U;
    } else if (key == "otel-trace-http-json") {
        options.traceOtlpHttpJson = parseBool(value) ? 1U : 0U;
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
        options.spdlogNativeConsole = parseBool(value);
    } else if (key == "spdlog-async") {
        options.spdlogAsync = parseBool(value);
    } else if (key == "spdlog-async-queue-size") {
        options.spdlogAsyncQueueSize = parseUInt32(value, options.spdlogAsyncQueueSize);
    } else if (key == "spdlog-async-thread-count") {
        options.spdlogAsyncThreadCount = parseUInt32(value, options.spdlogAsyncThreadCount);
    } else if (key == "spdlog-async-overflow-policy") {
        options.spdlogAsyncOverflowPolicy = value;
    }
}

auto basename(std::string_view path) -> std::string_view {
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string_view::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

auto env(const char* name) -> const char* {
    return std::getenv(name); // NOLINT(concurrency-mt-unsafe)
}

auto detectHostName() -> std::string {
    auto buffer = std::array<char, kHostNameBufferSize> {};
    if (gethostname(buffer.data(), buffer.size()) != 0) {
        return {};
    }
    if (buffer.back() != '\0') {
        return {};
    }
    return std::string{buffer.data()};
}

auto ensureHostName(TelemetryOptions& options) -> void {
    if (!options.hostName.empty()) {
        return;
    }
    options.hostName = detectHostName();
}

auto ensureServiceInstanceId(TelemetryOptions& options) -> void {
    if (!options.serviceInstanceId.empty()) {
        return;
    }
    options.serviceInstanceId = generateUuidString();
    options.generatedServiceInstanceId = true;
}

auto generateUuidString() -> std::string {
    return boost::uuids::to_string(boost::uuids::random_generator{}());
}

auto toLowerAscii(std::string_view value) -> std::string {
    auto lowered = std::string{value};
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return lowered;
}

auto normalizeServiceName(TelemetryOptions& options) -> void {
    options.serviceName = toLowerAscii(options.serviceName);
}

auto makeConfig(const TelemetryOptions& options) -> nestdaq_otel_config {
    nestdaq_otel_config config{};
    config.size = sizeof(config);
    config.logs = makeSignalConfig(options.logProtocol,
                                   options.logEndpointHttp,
                                   options.logEndpointGrpc,
                                   options.logHeaders,
                                   options.logOtlpHttpJson);
    config.metrics = makeSignalConfig(options.metricProtocol,
                                      options.metricEndpointHttp,
                                      options.metricEndpointGrpc,
                                      options.metricHeaders,
                                      options.metricOtlpHttpJson);
    config.traces = makeSignalConfig(options.traceProtocol,
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
    config.min_severity = parseFairLoggerSeverity(options.severity).value;
    config.timeout_ms = options.timeoutMs;
    config.metric_export_interval_ms = options.metricExportIntervalMs;
    return config;
}

auto makeSignalConfig(std::string_view protocol,
                      std::string_view endpoint_http,
                      std::string_view endpoint_grpc,
                      std::string_view headers,
                      uint32_t otlp_http_json) -> nestdaq_otel_signal_config {
    auto config = nestdaq_otel_signal_config{};
    config.protocol = protocol.data();
    config.endpoint_http = endpoint_http.data();
    config.endpoint_grpc = endpoint_grpc.data();
    config.headers = headers.data();
    config.otlp_http_json = otlp_http_json;
    return config;
}

auto parseBool(std::string_view value) -> bool {
    return value == "1" || value == "true" || value == "TRUE" ||
           value == "on" || value == "ON" || value == "yes" || value == "YES";
}

auto parseTelemetryOptions(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                           std::string_view default_service_name) -> TelemetryOptions {
    auto options = TelemetryOptions{};
    options.serviceName = default_service_name;
    if (argv == nullptr) {
        applyEnvironment(options);
        normalizeSpdlogAsyncOptions(options);
        ensureHostName(options);
        ensureServiceInstanceId(options);
        return options;
    }
    if (argc > 0) {
        if (const auto executable = basename(argv[0]); !executable.empty()) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            options.serviceName = executable;
        }
    }
    applyEnvironment(options);
    auto explicit_telemetry_service_name = false;
    auto explicit_telemetry_service_instance_id = false;

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
            assignOption(options, key, value);
            explicit_telemetry_service_name = true;
        } else if (key == "service-name") {
            if (!explicit_telemetry_service_name) {
                options.serviceName = value;
            }
        } else if (key == "otel-service-instance-id") {
            assignOption(options, key, value);
            options.generatedServiceInstanceId = false;
            explicit_telemetry_service_instance_id = true;
        } else if (key == "uuid") {
            if (!explicit_telemetry_service_instance_id) {
                options.serviceInstanceId = value;
                options.generatedServiceInstanceId = false;
            }
        } else {
            assignOption(options, key, value);
        }
    }

    normalizeServiceName(options);
    normalizeSpdlogAsyncOptions(options);
    ensureHostName(options);
    ensureServiceInstanceId(options);
    return options;
}

auto parseFairLoggerSeverity(std::string_view severity) -> SeverityParseResult {
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

auto parseUInt32(std::string_view value, uint32_t fallback) -> uint32_t {
    try {
        return static_cast<uint32_t>(std::stoul(std::string{value}));
    } catch (...) {
        return fallback;
    }
}

auto readTelemetryOptions(const boost::program_options::variables_map& vm,
                          std::string_view default_service_name) -> TelemetryOptions {
    auto options = TelemetryOptions{};
    options.serviceName = default_service_name;
    applyEnvironment(options);

    const auto read_string = [&vm, &options](std::string_view key) {
        const auto name = std::string{key};
        if (vm.count(name) != 0 && !vm[name].defaulted()) {
            assignOption(options, key, vm[name].as<std::string>());
        }
    };

    read_string("otel-library");
    read_string("otel-log-protocol");
    read_string("otel-metric-protocol");
    read_string("otel-trace-protocol");
    read_string("otel-log-endpoint-http");
    read_string("otel-log-endpoint-grpc");
    read_string("otel-metric-endpoint-http");
    read_string("otel-metric-endpoint-grpc");
    read_string("otel-trace-endpoint-http");
    read_string("otel-trace-endpoint-grpc");
    read_string("otel-log-headers");
    read_string("otel-metric-headers");
    read_string("otel-trace-headers");
    read_string("otel-log-severity");
    read_string("otel-service-name");
    read_string("otel-service-namespace");
    read_string("otel-service-instance-id");
    read_string("otel-fairmq-id");
    read_string("otel-fairmq-device");
    read_string("otel-fairmq-session");
    read_string("otel-fairmq-transport");
    read_string("spdlog-console-pattern");

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
    read_string("spdlog-async-overflow-policy");
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
    normalizeServiceName(options);
    normalizeSpdlogAsyncOptions(options);
    ensureHostName(options);
    ensureServiceInstanceId(options);
    return options;
}

auto warnUnknownSeverityFallback(std::string_view severity) -> void {
    if (!parseFairLoggerSeverity(severity).usedFallback) {
        return;
    }
    LOG(warn) << "Unknown otel-log-severity '" << severity << "', using FairLogger severity '"
              << fair::Logger::SeverityName(fair::Severity::info) << "'";
}

auto severityToFairLoggerValue(std::string_view severity) -> int32_t {
    return parseFairLoggerSeverity(severity).value;
}

auto setGeneratedUuidProperty(fair::mq::ProgOptions& config,
                              const TelemetryOptions& options,
                              std::string_view key) -> void {
    if (!options.generatedServiceInstanceId || options.serviceInstanceId.empty()) {
        return;
    }
    const auto property_key = std::string{key};
    if (config.Count(property_key) != 0) {
        return;
    }
    config.SetProperty<std::string>(property_key, options.serviceInstanceId);
}

TelemetryLibrary::~TelemetryLibrary() {
    shutdownTelemetry(kDefaultTimeoutMs);
    if (fHandle) {
        dlclose(fHandle);
    }
}

auto TelemetryLibrary::getLastError() const -> const std::string& {
    return fLastError;
}

auto TelemetryLibrary::initializeWith(const nestdaq_otel_config& config) -> bool {
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

auto TelemetryLibrary::forceFlush(uint64_t timeout_ms) -> bool {
    if (!fForceFlush) {
        return false;
    }
    return storeResult(fForceFlush(timeout_ms));
}

auto TelemetryLibrary::createSpdlogSink() const -> std::shared_ptr<spdlog::sinks::sink> {
    if (!fLogExportEnabled || !fCreateSpdlogSink) {
        return {};
    }
    return fCreateSpdlogSink();
}

auto TelemetryLibrary::recordFrameworkFairMQState(int64_t state_id, std::string_view state_name) -> void {
    if (!fRecordFrameworkFairMQState) {
        return;
    }
    const auto value = std::string{state_name};
    fRecordFrameworkFairMQState(state_id, value.data());
}

auto TelemetryLibrary::metricAddDoubleCounter(std::string_view name,
        double value,
        std::string_view unit,
        std::string_view description,
        const nestdaq_otel_attribute* attributes,
        uint64_t attribute_count) -> bool {
    if (!fMetricAddDoubleCounter) {
        return false;
    }
    return storeResult(fMetricAddDoubleCounter(name.data(), value, unit.data(), description.data(), attributes, attribute_count));
}

auto TelemetryLibrary::metricRecordDoubleHistogram(std::string_view name,
        double value,
        std::string_view unit,
        std::string_view description,
        const nestdaq_otel_attribute* attributes,
        uint64_t attribute_count) -> bool {
    if (!fMetricRecordDoubleHistogram) {
        return false;
    }
    return storeResult(fMetricRecordDoubleHistogram(name.data(), value, unit.data(), description.data(), attributes, attribute_count));
}

auto TelemetryLibrary::metricRecordDoubleGauge(std::string_view name,
        double value,
        std::string_view unit,
        std::string_view description,
        const nestdaq_otel_attribute* attributes,
        uint64_t attribute_count) -> bool {
    if (!fMetricRecordDoubleGauge) {
        return false;
    }
    return storeResult(fMetricRecordDoubleGauge(name.data(), value, unit.data(), description.data(), attributes, attribute_count));
}

auto TelemetryLibrary::load(const std::string& library) -> bool {
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

    fInitialize = resolveSymbol<int(const nestdaq_otel_config*)>(fHandle, "nestdaq_otel_init");
    fForceFlush = resolveSymbol<int(uint64_t)>(fHandle, "nestdaq_otel_force_flush");
    fCreateSpdlogSink = resolveSymbol<std::shared_ptr<spdlog::sinks::sink>()>(fHandle, "nestdaq_otel_create_spdlog_sink");
    fShutdown = resolveSymbol<int(uint64_t)>(fHandle, "nestdaq_otel_shutdown");
    fLastErrorFunction = resolveSymbol<const char*()>(fHandle, "nestdaq_otel_last_error");
    fRecordFrameworkFairMQState = resolveSymbol<void(int64_t, const char*)>(
                                      fHandle, "nestdaq_otel_framework_record_fairmq_state");
    fSetMinSeverity = resolveSymbol<int(int32_t)>(fHandle, "nestdaq_otel_set_min_severity");
    fSetNestdaqInstanceId = resolveSymbol<int(const char*)>(fHandle, "nestdaq_otel_set_nestdaq_instance_id");
    fMetricAddDoubleCounter = resolveSymbol<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
                                  fHandle, "nestdaq_otel_metric_add_double_counter");
    fMetricRecordDoubleHistogram = resolveSymbol<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
                                       fHandle, "nestdaq_otel_metric_record_double_histogram");
    fMetricRecordDoubleGauge = resolveSymbol<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)>(
                                   fHandle, "nestdaq_otel_metric_record_double_gauge");
    fSpanEnd = resolveSymbol<int(uint64_t)>(fHandle, "nestdaq_otel_span_end");
    fSpanSetAttribute = resolveSymbol<int(uint64_t, const nestdaq_otel_attribute*)>(fHandle, "nestdaq_otel_span_set_attribute");
    fSpanStart = resolveSymbol<uint64_t(const char*, const nestdaq_otel_attribute*, uint64_t)>(fHandle, "nestdaq_otel_span_start");

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

auto TelemetryLibrary::spanEnd(uint64_t span_handle) -> bool {
    if (!fSpanEnd) {
        return false;
    }
    return storeResult(fSpanEnd(span_handle));
}

auto TelemetryLibrary::spanSetAttribute(uint64_t span_handle, const nestdaq_otel_attribute& attribute) -> bool {
    if (!fSpanSetAttribute) {
        return false;
    }
    return storeResult(fSpanSetAttribute(span_handle, &attribute));
}

auto TelemetryLibrary::setNestdaqInstanceId(std::string_view instance_id) -> bool {
    if (!fSetNestdaqInstanceId) {
        return false;
    }
    const auto value = std::string{instance_id};
    return storeResult(fSetNestdaqInstanceId(value.data()));
}

auto TelemetryLibrary::spanStart(std::string_view name,
                                 const nestdaq_otel_attribute* attributes,
                                 uint64_t attribute_count) -> uint64_t {
    if (!fSpanStart) {
        return 0;
    }
    const auto span_handle = fSpanStart(name.data(), attributes, attribute_count);
    if (span_handle == 0) {
        storeResult(NESTDAQ_OTEL_ERROR);
    } else {
        fLastError.clear();
    }
    return span_handle;
}

auto TelemetryLibrary::setMinSeverity(std::string_view severity) -> bool {
    const auto parsed_severity = parseFairLoggerSeverity(severity);
    const auto updated = setMinSeverity(parsed_severity.value);
    if (updated && parsed_severity.usedFallback) {
        warnUnknownSeverityFallback(severity);
    }
    return updated;
}

auto TelemetryLibrary::setMinSeverity(int32_t severity) -> bool {
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

auto TelemetryLibrary::shutdownTelemetry(uint64_t timeout_ms) const -> void {
    fLogExportEnabled = false;
    if (fShutdown && !fShutdownCalled) {
        fShutdownCalled = true;
        fShutdown(timeout_ms);
    }
}

auto TelemetryLibrary::storeResult(int rc) -> bool {
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

auto subscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config,
                                     TelemetryLibrary& telemetry) -> void {
    config.SubscribeAsString(std::string{kTelemetryConfigSubscriber},
    [&telemetry](const fair::mq::PropertyChange::KeyType& key, std::string value) {
        if (key == "id") {
            if (!telemetry.setNestdaqInstanceId(value)) {
                LOG(error) << "Failed to update OTel NestDAQ instance id: "
                           << telemetry.getLastError();
            }
            return;
        }
        if (key == "otel-log-severity" && !telemetry.setMinSeverity(value)) {
            LOG(error) << "Failed to update OTel log severity: "
                       << telemetry.getLastError();
        }
    });
}

auto unsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void {
    config.UnsubscribeAsString(std::string{kTelemetryConfigSubscriber});
}

} // namespace nestdaq::telemetry
