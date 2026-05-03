#pragma once

#include <fairmq/DeviceRunner.h>
#include <fairmq/Version.h>

#include <boost/program_options.hpp>

#include <nestdaq/telemetry/OpenTelemetryInitializer.h>

#include <dlfcn.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using FairMQDevicePtr = std::unique_ptr<fair::mq::Device>;

// To be implemented by the user to return a child class of fair::mq::Device.
FairMQDevicePtr getDevice(const fair::mq::ProgOptions& config);

// To be implemented by the user to add custom command line options.
void addCustomOptions(boost::program_options::options_description&);

namespace nestdaq::run_device_detail {

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

struct ProgramArguments {
    std::vector<std::string> storage;
    std::vector<char*> argv;

    auto argc() const -> int
    {
        return static_cast<int>(argv.size());
    }
};

auto NormalizeArguments(int argc, char* argv[]) -> ProgramArguments // NOLINT(cppcoreguidelines-avoid-c-arrays)
{
    auto arguments = ProgramArguments{};
    arguments.storage.reserve(static_cast<std::size_t>(argc));
    arguments.argv.reserve(static_cast<std::size_t>(argc));

    for (int i = 0; i < argc; ++i) {
        const auto arg = std::string_view{argv[i]}; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg == "--otel-log-protocol=") {
            arguments.storage.emplace_back("--otel-log-protocol");
        } else {
            arguments.storage.emplace_back(argv[i]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        }
    }

    for (auto& arg : arguments.storage) {
        arguments.argv.emplace_back(arg.data());
    }

    return arguments;
}

auto Env(const char* name) -> const char*
{
    return std::getenv(name); // NOLINT(concurrency-mt-unsafe)
}

auto ParseBool(std::string_view value) -> bool
{
    return value == "1" || value == "true" || value == "TRUE" ||
           value == "on" || value == "ON" || value == "yes" || value == "YES";
}

auto ParseUInt32(std::string_view value, uint32_t fallback) -> uint32_t
{
    try {
        return static_cast<uint32_t>(std::stoul(std::string{value}));
    } catch (...) {
        return fallback;
    }
}

auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t
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

auto AssignOption(TelemetryOptions& options,
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

auto ParseTelemetryOptions(int argc, char* argv[]) -> TelemetryOptions // NOLINT(cppcoreguidelines-avoid-c-arrays)
{
    TelemetryOptions options;

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

    if (!options.endpoint.empty()) {
        if (!options.endpointHttpSet) {
            options.endpointHttp = options.endpoint;
        }
        if (!options.endpointGrpcSet) {
            options.endpointGrpc = options.endpoint;
        }
    }

    return options;
}

auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config_v1
{
    nestdaq_otel_config_v1 config{};
    config.size = sizeof(config);
    config.protocol = options.protocol.c_str();
    config.endpoint = options.endpoint.c_str();
    config.endpoint_http = options.endpointHttp.c_str();
    config.endpoint_grpc = options.endpointGrpc.c_str();
    config.headers = options.headers.c_str();
    config.service_name = options.serviceName.c_str();
    config.service_namespace = options.serviceNamespace.c_str();
    config.service_instance_id = options.serviceInstanceId.c_str();
    config.fairmq_id = options.fairmqId.c_str();
    config.fairmq_device = options.fairmqDevice.c_str();
    config.fairmq_session = options.fairmqSession.c_str();
    config.fairmq_transport = options.fairmqTransport.c_str();
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

    auto Load(const std::string& library) -> bool
    {
        fHandle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!fHandle) {
            fLastError = dlerror(); // NOLINT(concurrency-mt-unsafe)
            return false;
        }

        fInitialize = Resolve<int (*)(const nestdaq_otel_config_v1*)>("nestdaq_otel_init_v1");
        fShutdown = Resolve<int (*)(uint64_t)>("nestdaq_otel_shutdown");
        fLastErrorFunction = Resolve<const char* (*)()>("nestdaq_otel_last_error");

        if (!fInitialize || !fShutdown) {
            fLastError = "telemetry library does not export the required nestdaq_otel_* C ABI";
            dlclose(fHandle);
            fHandle = nullptr;
            fInitialize = nullptr;
            fShutdown = nullptr;
            fLastErrorFunction = nullptr;
            return false;
        }

        fLastError.clear();
        return true;
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

    auto ShutdownTelemetry(uint64_t timeoutMs) const -> void
    {
        if (fShutdown && !fShutdownCalled) {
            fShutdownCalled = true;
            fShutdown(timeoutMs);
        }
    }

    auto GetLastError() const -> const std::string&
    {
        return fLastError;
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
    mutable bool fShutdownCalled{false};
    std::string fLastError;
};

auto AddTelemetryOptions(boost::program_options::options_description& options) -> void
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
           ("otel-service-name", bpo::value<std::string>()->default_value("nestdaq"), "OTel service.name resource attribute")
           ("otel-service-namespace", bpo::value<std::string>(), "OTel service.namespace resource attribute")
           ("otel-service-instance-id", bpo::value<std::string>(), "OTel service.instance.id resource attribute")
           ("otel-fairmq-id", bpo::value<std::string>(), "FairMQ id resource attribute")
           ("otel-fairmq-device", bpo::value<std::string>(), "FairMQ device resource attribute")
           ("otel-fairmq-session", bpo::value<std::string>(), "FairMQ session resource attribute")
           ("otel-fairmq-transport", bpo::value<std::string>(), "FairMQ transport resource attribute");
}

} // namespace nestdaq::run_device_detail

int main(int argc, char* argv[])
{
    using namespace fair::mq;
    using namespace fair::mq::hooks;

    try {
        auto arguments = nestdaq::run_device_detail::NormalizeArguments(argc, argv);
        const auto telemetryOptions =
            nestdaq::run_device_detail::ParseTelemetryOptions(arguments.argc(), arguments.argv.data());
        auto telemetry = std::make_unique<nestdaq::run_device_detail::TelemetryLibrary>();
        auto telemetryLoaded = false;

        if (!telemetryOptions.library.empty()) {
            telemetryLoaded = telemetry->Load(telemetryOptions.library);
            if (!telemetryLoaded) {
                std::cerr << "Failed to load telemetry library '" << telemetryOptions.library
                          << "': " << telemetry->GetLastError() << '\n';
                if (telemetryOptions.required) {
                    return 1;
                }
            } else {
                const auto config = nestdaq::run_device_detail::MakeConfig(telemetryOptions);
                if (!telemetry->InitializeWith(config)) {
                    std::cerr << "Failed to initialize telemetry library '" << telemetryOptions.library
                              << "': " << telemetry->GetLastError() << '\n';
                    if (telemetryOptions.required) {
                        return 1;
                    }
                }
            }
        }

        DeviceRunner runner{arguments.argc(), arguments.argv.data(), false};

        runner.AddHook<SetCustomCmdLineOptions>([](DeviceRunner& r) {
            boost::program_options::options_description customOptions("Custom options");
            addCustomOptions(customOptions);
            r.fConfig.AddToCmdLineOptions(customOptions);

            boost::program_options::options_description otelOptions("OpenTelemetry options");
            nestdaq::run_device_detail::AddTelemetryOptions(otelOptions);
            r.fConfig.AddToCmdLineOptions(otelOptions);
        });

        runner.AddHook<InstantiateDevice>([](DeviceRunner& r) {
            r.fDevice = getDevice(r.fConfig);
        });

        const auto rc = runner.Run();
        if (telemetryLoaded) {
            telemetry->ShutdownTelemetry(telemetryOptions.timeoutMs);
        }
        return rc;
    } catch (std::exception& e) {
        LOG(error) << "Uncaught exception reached the top of main: " << e.what();
        return 1;
    } catch (...) {
        LOG(error) << "Uncaught exception reached the top of main.";
        return 1;
    }
}
