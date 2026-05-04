#pragma once

#include <fairmq/DeviceRunner.h>
#include <fairlogger/Logger.h>

#include <boost/program_options.hpp>

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>

#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// To be implemented by the user to add custom command line options.
void addCustomOptions(boost::program_options::options_description& options);

// To be implemented by the user to return a child class of fair::mq::Device.
std::unique_ptr<fair::mq::Device> getDevice(const fair::mq::ProgOptions& config);

namespace nestdaq::run_device_detail {

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

} // namespace nestdaq::run_device_detail

int main(int argc, char* argv[])
{
    using namespace fair::mq;
    using namespace fair::mq::hooks;

    try {
        auto arguments = nestdaq::run_device_detail::NormalizeArguments(argc, argv);
        const auto telemetryOptions =
            nestdaq::telemetry::ParseTelemetryOptions(arguments.argc(), arguments.argv.data(), "nestdaq");
        auto telemetry = std::make_unique<nestdaq::telemetry::TelemetryLibrary>();
        auto telemetryLoaded = false;
        auto telemetryInitialized = false;

        if (!telemetryOptions.library.empty()) {
            telemetryLoaded = telemetry->Load(telemetryOptions.library);
            if (!telemetryLoaded) {
                std::cerr << "Failed to load telemetry library '" << telemetryOptions.library
                          << "': " << telemetry->GetLastError() << '\n';
                if (telemetryOptions.required) {
                    return 1;
                }
            } else {
                const auto config = nestdaq::telemetry::MakeConfig(telemetryOptions);
                if (!telemetry->InitializeWith(config)) {
                    std::cerr << "Failed to initialize telemetry library '" << telemetryOptions.library
                              << "': " << telemetry->GetLastError() << '\n';
                    if (telemetryOptions.required) {
                        return 1;
                    }
                } else {
                    telemetryInitialized = true;
                }
            }
        }

        DeviceRunner runner{arguments.argc(), arguments.argv.data(), false};

        runner.AddHook<SetCustomCmdLineOptions>([telemetryInitialized, telemetry = telemetry.get()](DeviceRunner& r) {
            boost::program_options::options_description customOptions("Custom options");
            addCustomOptions(customOptions);
            r.fConfig.AddToCmdLineOptions(customOptions);

            boost::program_options::options_description otelOptions("OpenTelemetry options");
            nestdaq::telemetry::AddTelemetryOptions(otelOptions, "nestdaq");
            r.fConfig.AddToCmdLineOptions(otelOptions);

            if (telemetryInitialized) {
                nestdaq::telemetry::SubscribeTelemetryOptionChanges(r.fConfig, *telemetry);
            }
        });

        runner.AddHook<InstantiateDevice>([](DeviceRunner& r) {
            r.fDevice = getDevice(r.fConfig);
        });

        const auto rc = runner.Run();
        if (telemetryInitialized) {
            nestdaq::telemetry::UnsubscribeTelemetryOptionChanges(runner.fConfig);
        }
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
