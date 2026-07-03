/** @file
 *  @brief Implements a minimal NestDAQ device example.
 */

#include <chrono>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

#include <nestdaq/runDevice.h>

#include "NullDevice.h"

#if __has_include(<spdlog/spdlog.h>)
#include <spdlog/spdlog.h>
#endif
#if __has_include(<spdlog/spdlog.h>) && __has_include(<nestdaq/telemetry/SpdlogLogger.h>)
#include <nestdaq/telemetry/SpdlogLogger.h>
#elif __has_include(<spdlog/spdlog.h>)
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

static constexpr std::string_view kMyClass{"NullDevice"};

namespace bpo = boost::program_options;

void addCustomOptions(bpo::options_description &options)
{
}

std::unique_ptr<fair::mq::Device> getDevice(const fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<NullDevice>();
}

void NullDevice::Bind()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

bool NullDevice::ConditionalRun()
{
    LOG(info) << __PRETTY_FUNCTION__;
    return true;
}

void NullDevice::Connect()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

void NullDevice::Init()
{
#if __has_include(<spdlog/spdlog.h>) && __has_include(<nestdaq/telemetry/SpdlogLogger.h>)
    if (!fLogger) {
        fLogger = nestdaq::telemetry::CreateSpdlogLogger("NullDevice");
    }
    fLogger->info("NullDevice example spdlog log");
#elif __has_include(<spdlog/spdlog.h>)
    if (!fLogger) {
        if (nestdaq::telemetry::GetSpdlogNativeConsoleEnabled()) {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_pattern(nestdaq::telemetry::GetSpdlogConsolePattern());
            fLogger = std::make_shared<spdlog::logger>(
                          "NullDevice",
                          spdlog::sinks_init_list{std::move(consoleSink)});
        }
    }
    if (fLogger) {
        fLogger->info("NullDevice example spdlog log");
    }
#endif
    LOG(info) << __PRETTY_FUNCTION__;
}

void NullDevice::InitTask()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

void NullDevice::PostRun()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

void NullDevice::PreRun()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

void NullDevice::Reset()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

void NullDevice::ResetTask()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

void NullDevice::Run()
{
    LOG(info) << __PRETTY_FUNCTION__;
}
