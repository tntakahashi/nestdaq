#pragma once

#include <fairmq/ProgOptions.h>
#include <fairmq/Version.h>

#include <boost/program_options.hpp>

#include <nestdaq/telemetry/OpenTelemetryInitializer.h>

#include <dlfcn.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>

namespace nestdaq::telemetry {

static constexpr std::string_view kDefaultTelemetryLibrary{"libnestdaq_fairlogger_otel.so"};
static constexpr std::string_view kDefaultProtocol{"console"};
static constexpr std::string_view kDefaultHttpEndpoint{"http://localhost:4318/v1/logs"};
static constexpr std::string_view kDefaultGrpcEndpoint{"localhost:4317"};
static constexpr uint32_t kDefaultTimeoutMs{5000};
static constexpr int32_t kSeverityNoLog{0};
static constexpr int32_t kSeverityTrace{1};
static constexpr int32_t kSeverityDebug4{2};
static constexpr int32_t kSeverityDebug3{3};
static constexpr int32_t kSeverityDebug2{4};
static constexpr int32_t kSeverityDebug1{5};
static constexpr int32_t kSeverityDebug{6};
static constexpr int32_t kSeverityDetail{7};
static constexpr int32_t kSeverityInfo{8};
static constexpr int32_t kSeverityState{9};
static constexpr int32_t kSeverityWarn{10};
static constexpr int32_t kSeverityImportant{11};
static constexpr int32_t kSeverityAlarm{12};
static constexpr int32_t kSeverityError{13};
static constexpr int32_t kSeverityCritical{14};
static constexpr int32_t kSeverityFatal{15};
static constexpr std::string_view kTelemetryConfigSubscriber{"nestdaq-telemetry"};

struct TelemetryOptions {
    std::string library{kDefaultTelemetryLibrary};
    std::string protocol{kDefaultProtocol};
    std::string endpoint;
    std::string endpointHttp{kDefaultHttpEndpoint};
    std::string endpointGrpc{kDefaultGrpcEndpoint};
    std::string headers;
    std::string severity{"info"};
    std::string serviceName{"nestdaq"};
    std::string serviceNamespace;
    std::string serviceInstanceId;
    std::string fairmqId;
    std::string fairmqDevice;
    std::string fairmqSession;
    std::string fairmqTransport;
    uint32_t timeoutMs{kDefaultTimeoutMs};
    uint32_t otlpHttpJson{1};
    bool required{false};
    bool endpointHttpSet{false};
    bool endpointGrpcSet{false};
};

class TelemetryLibrary;

inline auto AddTelemetryOptions(boost::program_options::options_description& options,
                                std::string_view defaultServiceName = "nestdaq") -> void;
inline auto ApplyEnvironment(TelemetryOptions& options) -> void;
inline auto AssignOption(TelemetryOptions& options,
                         std::string_view key,
                         std::string_view value) -> void;
inline auto Env(const char* name) -> const char*;
inline auto FinalizeEndpoints(TelemetryOptions& options) -> void;
inline auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config_v1;
inline auto ParseBool(std::string_view value) -> bool;
inline auto ParseTelemetryOptions(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                                  std::string_view defaultServiceName = "nestdaq") -> TelemetryOptions;
inline auto ParseUInt32(std::string_view value, uint32_t fallback) -> uint32_t;
inline auto ReadTelemetryOptions(const boost::program_options::variables_map& vm,
                                 std::string_view defaultServiceName) -> TelemetryOptions;
inline auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t;
inline auto SubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config,
                                            TelemetryLibrary& telemetry) -> void;
inline auto UnsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void;

inline auto AddTelemetryOptions(boost::program_options::options_description& options,
                                std::string_view defaultServiceName) -> void
{
    namespace bpo = boost::program_options;
    options.add_options()
           ("otel-log-library", bpo::value<std::string>()->default_value(std::string{kDefaultTelemetryLibrary}), "Telemetry shared library path or soname to dlopen")
           ("otel-log-protocol", bpo::value<std::string>()->default_value(std::string{kDefaultProtocol})->implicit_value(""), "Comma-separated OTel log exporter protocols to enable: console, otlp-http, otlp-grpc. Empty disables OTel output")
           ("otel-log-endpoint", bpo::value<std::string>(), "Compatibility OTel collector endpoint for both HTTP and gRPC logs")
           ("otel-log-endpoint-http", bpo::value<std::string>()->default_value(std::string{kDefaultHttpEndpoint}), "OTLP HTTP logs endpoint")
           ("otel-log-endpoint-grpc", bpo::value<std::string>()->default_value(std::string{kDefaultGrpcEndpoint}), "OTLP gRPC logs endpoint")
           ("otel-log-headers", bpo::value<std::string>(), "OTel exporter headers as comma-separated key=value pairs")
           ("otel-log-severity", bpo::value<std::string>()->default_value("info"), "Minimum severity exported to OTel")
           ("otel-log-required", bpo::value<bool>()->default_value(false), "Fail startup if telemetry library cannot be loaded")
           ("otel-log-timeout-ms", bpo::value<uint32_t>()->default_value(kDefaultTimeoutMs), "OTel force-flush/shutdown timeout in milliseconds")
           ("otel-log-http-json", bpo::value<bool>()->default_value(true), "Use JSON content type for OTLP HTTP logs")
           ("otel-service-name", bpo::value<std::string>()->default_value(std::string{defaultServiceName}), "OTel service.name resource attribute")
           ("otel-service-namespace", bpo::value<std::string>(), "OTel service.namespace resource attribute")
           ("otel-service-instance-id", bpo::value<std::string>(), "OTel service.instance.id resource attribute")
           ("otel-fairmq-id", bpo::value<std::string>(), "FairMQ id resource attribute")
           ("otel-fairmq-device", bpo::value<std::string>(), "FairMQ device resource attribute")
           ("otel-fairmq-session", bpo::value<std::string>(), "FairMQ session resource attribute")
           ("otel-fairmq-transport", bpo::value<std::string>(), "FairMQ transport resource attribute");
}

inline auto ApplyEnvironment(TelemetryOptions& options) -> void
{
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_LIBRARY")) {
        options.library = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_PROTOCOL")) {
        options.protocol = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_ENDPOINT")) {
        options.endpoint = value;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_ENDPOINT_HTTP")) {
        options.endpointHttp = value;
        options.endpointHttpSet = true;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_ENDPOINT_GRPC")) {
        options.endpointGrpc = value;
        options.endpointGrpcSet = true;
    }
    if (const auto* value = Env("NESTDAQ_OTEL_LOG_HEADERS")) {
        options.headers = value;
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
    if (key == "otel-log-library") {
        options.library = value;
    } else if (key == "otel-log-protocol") {
        options.protocol = value;
    } else if (key == "otel-log-endpoint") {
        options.endpoint = value;
    } else if (key == "otel-log-endpoint-http") {
        options.endpointHttp = value;
        options.endpointHttpSet = true;
    } else if (key == "otel-log-endpoint-grpc") {
        options.endpointGrpc = value;
        options.endpointGrpcSet = true;
    } else if (key == "otel-log-headers") {
        options.headers = value;
    } else if (key == "otel-log-severity") {
        options.severity = value;
    } else if (key == "otel-log-required") {
        options.required = ParseBool(value);
    } else if (key == "otel-log-timeout-ms") {
        options.timeoutMs = ParseUInt32(value, options.timeoutMs);
    } else if (key == "otel-log-http-json") {
        options.otlpHttpJson = ParseBool(value) ? 1U : 0U;
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

inline auto Env(const char* name) -> const char*
{
    return std::getenv(name); // NOLINT(concurrency-mt-unsafe)
}

inline auto FinalizeEndpoints(TelemetryOptions& options) -> void
{
    if (!options.endpoint.empty()) {
        if (!options.endpointHttpSet) {
            options.endpointHttp = options.endpoint;
        }
        if (!options.endpointGrpcSet) {
            options.endpointGrpc = options.endpoint;
        }
    }
}

inline auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config_v1
{
    nestdaq_otel_config_v1 config{};
    config.size = sizeof(config);
    config.protocol = options.protocol.data();
    config.endpoint = options.endpoint.data();
    config.endpoint_http = options.endpointHttp.data();
    config.endpoint_grpc = options.endpointGrpc.data();
    config.headers = options.headers.data();
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
    config.min_severity = SeverityToFairLoggerValue(options.severity);
    config.timeout_ms = options.timeoutMs;
    config.otlp_http_json = options.otlpHttpJson;
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
    ApplyEnvironment(options);

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
        } else if (key.rfind("otel-", 0) == 0 && i + 1 < argc &&
                   std::string_view{argv[i + 1]}.rfind("--", 0) != 0) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            value = argv[++i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        } else if (key == "otel-log-protocol") {
            value = "";
        } else {
            continue;
        }
        AssignOption(options, key, value);
    }

    FinalizeEndpoints(options);
    return options;
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

    readString("otel-log-library");
    readString("otel-log-protocol");
    readString("otel-log-endpoint");
    readString("otel-log-endpoint-http");
    readString("otel-log-endpoint-grpc");
    readString("otel-log-headers");
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
    if (vm.count("otel-log-timeout-ms") != 0) {
        options.timeoutMs = vm["otel-log-timeout-ms"].as<uint32_t>();
    }
    if (vm.count("otel-log-http-json") != 0) {
        options.otlpHttpJson = vm["otel-log-http-json"].as<bool>() ? 1U : 0U;
    }

    FinalizeEndpoints(options);
    return options;
}

inline auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t
{
    if (severity == "nolog") {
        return kSeverityNoLog;
    }
    if (severity == "trace") {
        return kSeverityTrace;
    }
    if (severity == "debug4") {
        return kSeverityDebug4;
    }
    if (severity == "debug3") {
        return kSeverityDebug3;
    }
    if (severity == "debug2") {
        return kSeverityDebug2;
    }
    if (severity == "debug1") {
        return kSeverityDebug1;
    }
    if (severity == "debug") {
        return kSeverityDebug;
    }
    if (severity == "detail") {
        return kSeverityDetail;
    }
    if (severity == "info") {
        return kSeverityInfo;
    }
    if (severity == "state") {
        return kSeverityState;
    }
    if (severity == "warn" || severity == "warning") {
        return kSeverityWarn;
    }
    if (severity == "important") {
        return kSeverityImportant;
    }
    if (severity == "alarm") {
        return kSeverityAlarm;
    }
    if (severity == "error") {
        return kSeverityError;
    }
    if (severity == "critical") {
        return kSeverityCritical;
    }
    if (severity == "fatal") {
        return kSeverityFatal;
    }
    return kSeverityInfo;
}

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

    auto GetLastError() const -> const std::string&
    {
        return fLastError;
    }

    auto InitializeWith(const nestdaq_otel_config_v1& config) -> bool
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

    auto Load(const std::string& library) -> bool
    {
        fHandle = dlopen(library.data(), RTLD_NOW | RTLD_LOCAL);
        if (!fHandle) {
            fLastError = dlerror(); // NOLINT(concurrency-mt-unsafe)
            return false;
        }

        fInitialize = Resolve<int (*)(const nestdaq_otel_config_v1*)>("nestdaq_otel_init_v1");
        fShutdown = Resolve<int (*)(uint64_t)>("nestdaq_otel_shutdown");
        fLastErrorFunction = Resolve<const char* (*)()>("nestdaq_otel_last_error");
        fSetMinSeverity = Resolve<int (*)(int32_t)>("nestdaq_otel_set_min_severity");

        if (!fInitialize || !fShutdown) {
            fLastError = "telemetry library does not export the required nestdaq_otel_* C ABI";
            dlclose(fHandle);
            fHandle = nullptr;
            fInitialize = nullptr;
            fShutdown = nullptr;
            fLastErrorFunction = nullptr;
            fSetMinSeverity = nullptr;
            return false;
        }

        fLastError.clear();
        return true;
    }

    auto SetMinSeverity(std::string_view severity) -> bool
    {
        return SetMinSeverity(SeverityToFairLoggerValue(severity));
    }

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

    void* fHandle{nullptr};
    std::function<int(const nestdaq_otel_config_v1*)> fInitialize;
    std::function<int(uint64_t)> fShutdown;
    std::function<const char*()> fLastErrorFunction;
    std::function<int(int32_t)> fSetMinSeverity;
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
                                     std::cerr << "Failed to update OTel log severity: "
                                               << telemetry.GetLastError() << '\n';
                                 }
                             });
}

inline auto UnsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void
{
    config.UnsubscribeAsString(std::string{kTelemetryConfigSubscriber});
}

} // namespace nestdaq::telemetry
