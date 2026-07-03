#pragma once

/**
 * @file runDevice.h
 * @brief NestDAQ replacement entry point for FairMQ devices.
 *
 * Including this header defines `main()` and expects the application to provide
 * `addCustomOptions()` and `getDevice()`. The wrapper installs NestDAQ
 * telemetry options before constructing and running the FairMQ device.
 */

#include <fairmq/DeviceRunner.h>
#include <fairlogger/Logger.h>

#include <boost/program_options.hpp>

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>
#include <nestdaq/telemetry/Telemetry.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Add application-specific command-line options.
 *
 * The function must be implemented by the executable that includes
 * `nestdaq/runDevice.h`.
 */
void addCustomOptions(boost::program_options::options_description& options);

/**
 * @brief Create the FairMQ device instance for the application.
 *
 * The function must return exclusive ownership of a `fair::mq::Device`
 * subclass. It is called after command-line options have been registered.
 */
std::unique_ptr<fair::mq::Device> getDevice(const fair::mq::ProgOptions& config);

namespace nestdaq::run_device_detail {

inline constexpr std::string_view kTelemetryStateSubscriber{"nestdaq-otel-framework-state"};

struct ProgramArguments {
    std::vector<std::string> storage;
    std::vector<char*> argv;

    auto argc() const -> int {
        return static_cast<int>(argv.size());
    }
};

auto normalizeArguments(int argc, char* argv[]) -> ProgramArguments { // NOLINT(cppcoreguidelines-avoid-c-arrays)
    auto arguments = ProgramArguments{};
    arguments.storage.reserve(static_cast<std::size_t>(argc));
    arguments.argv.reserve(static_cast<std::size_t>(argc));

    // Keep normalized arguments in owned storage so both telemetry option
    // parsing and FairMQ DeviceRunner receive stable argv pointers.
    for (int i = 0; i < argc; ++i) {
        const auto arg = std::string_view{argv[i]}; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg == "--otel-log-protocol=") {
            // Treat an explicit empty log protocol as the option's implicit
            // empty value before handing the same argv to FairMQ.
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

} // namespace nestdaq::run_device_detail

int main(int argc, char* argv[]) {
    using namespace fair::mq;
    using namespace fair::mq::hooks;

    try {
        auto arguments = nestdaq::run_device_detail::normalizeArguments(argc, argv);
        const auto telemetry_options =
            nestdaq::telemetry::ParseTelemetryOptions(arguments.argc(), arguments.argv.data(), "nestdaq");
        nestdaq::telemetry::SetSpdlogConsolePattern(telemetry_options.spdlogConsolePattern);
        nestdaq::telemetry::SetSpdlogNativeConsoleEnabled(telemetry_options.spdlogNativeConsole);
        nestdaq::telemetry::SetSpdlogAsyncOptions({
            .enabled = telemetry_options.spdlogAsync,
            .queueSize = telemetry_options.spdlogAsyncQueueSize,
            .threadCount = telemetry_options.spdlogAsyncThreadCount,
            .overflowPolicy = telemetry_options.spdlogAsyncOverflowPolicy,
        });
        auto telemetry = std::make_unique<nestdaq::telemetry::TelemetryLibrary>();
        auto telemetry_loaded = false;
        auto telemetry_initialized = false;
        auto telemetry_resolved = false;

        if (!telemetry_options.library.empty()) {
            telemetry_loaded = telemetry->Load(telemetry_options.library);
            if (!telemetry_loaded) {
                LOG(error) << "Failed to load telemetry library '" << telemetry_options.library
                           << "': " << telemetry->GetLastError();
                if (telemetry_options.required) {
                    return EXIT_FAILURE;
                }
            } else {
                auto unresolved_options = telemetry_options;
                unresolved_options.metricProtocol.clear();
                unresolved_options.traceProtocol.clear();
                unresolved_options.nestdaqInstanceId.clear();
                unresolved_options.nestdaqInstanceIdStatus = "unresolved";
                const auto config = nestdaq::telemetry::MakeConfig(unresolved_options);
                if (!telemetry->InitializeWith(config)) {
                    LOG(error) << "Failed to initialize telemetry library '" << telemetry_options.library
                               << "': " << telemetry->GetLastError();
                    if (telemetry_options.required) {
                        return EXIT_FAILURE;
                    }
                } else {
                    telemetry_initialized = true;
                    nestdaq::telemetry::WarnUnknownSeverityFallback(telemetry_options.severity);
                }
            }
        }

        DeviceRunner runner{arguments.argc(), arguments.argv.data(), false};

        runner.AddHook<SetCustomCmdLineOptions>([telemetry_initialized, telemetry = telemetry.get()](DeviceRunner& r) {
            boost::program_options::options_description customOptions("Custom options");
            addCustomOptions(customOptions);
            r.fConfig.AddToCmdLineOptions(customOptions);

            boost::program_options::options_description otelOptions("OpenTelemetry options");
            nestdaq::telemetry::AddTelemetryOptions(otelOptions, "nestdaq");
            r.fConfig.AddToCmdLineOptions(otelOptions);

            if (telemetry_initialized) {
                nestdaq::telemetry::SubscribeTelemetryOptionChanges(r.fConfig, *telemetry);
            }
        });

        runner.AddHook<InstantiateDevice>([&telemetry_options,
                                           &telemetry_resolved,
                                           telemetry_initialized,
        telemetry = telemetry.get()](DeviceRunner& r) {
            nestdaq::telemetry::SetGeneratedUuidProperty(r.fConfig, telemetry_options);
            r.fDevice = getDevice(r.fConfig);
            if (telemetry_initialized && r.fConfig.Count("id") != 0) {
                auto resolvedOptions = telemetry_options;
                resolvedOptions.nestdaqInstanceId = r.fConfig.GetProperty<std::string>("id");
                resolvedOptions.nestdaqInstanceIdStatus = "resolved";
                const auto config = nestdaq::telemetry::MakeConfig(resolvedOptions);
                if (!telemetry->InitializeWith(config)) {
                    LOG(error) << "Failed to reinitialize telemetry with NestDAQ instance id '"
                               << resolvedOptions.nestdaqInstanceId << "': " << telemetry->GetLastError();
                    if (telemetry_options.required) {
                        throw std::runtime_error{"failed to reinitialize required telemetry"};
                    }
                } else {
                    telemetry_resolved = true;
                    nestdaq::telemetry::SetActiveTelemetryLibrary(telemetry);
                    telemetry->SetNestdaqInstanceId(resolvedOptions.nestdaqInstanceId);
                }
            }
            if (telemetry_initialized && telemetry_resolved && r.fDevice) {
                r.fDevice->SubscribeToStateChange(
                    std::string{nestdaq::run_device_detail::kTelemetryStateSubscriber},
                [telemetry](const fair::mq::State newState) {
                    telemetry->RecordFrameworkFairMQState(
                        static_cast<int64_t>(newState),
                        fair::mq::GetStateName(newState));
                });
            }
        });

        const auto rc = runner.Run();
        if (telemetry_initialized && telemetry_resolved && runner.fDevice) {
            runner.fDevice->UnsubscribeFromStateChange(
                std::string{nestdaq::run_device_detail::kTelemetryStateSubscriber});
        }
        if (telemetry_initialized) {
            nestdaq::telemetry::UnsubscribeTelemetryOptionChanges(runner.fConfig);
        }
        if (telemetry_loaded) {
            nestdaq::telemetry::SetActiveTelemetryLibrary(nullptr);
            telemetry->ShutdownTelemetry(telemetry_options.timeoutMs);
        }
        return rc;
    } catch (std::exception& e) {
        LOG(error) << "Uncaught exception reached the top of main: " << e.what();
        return EXIT_FAILURE;
    } catch (...) {
        LOG(error) << "Uncaught exception reached the top of main.";
        return EXIT_FAILURE;
    }
}
