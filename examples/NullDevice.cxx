/** @file
 *  @brief Implements a minimal NestDAQ device example.
 */

#include <chrono>
#include <memory>
#include <string_view>
#include <thread>

#include <nestdaq/runDevice.h>

#if defined(NESTDAQ_EXAMPLES_HAVE_SPDLOG_OTEL) && __has_include(<nestdaq/telemetry/SpdlogOpenTelemetrySink.h>) && __has_include(<spdlog/spdlog.h>)
#include <nestdaq/telemetry/SpdlogOpenTelemetrySink.h>
#include <spdlog/spdlog.h>
#define NESTDAQ_EXAMPLES_USE_SPDLOG_OTEL
#endif

#include "NullDevice.h"

static constexpr std::string_view kMyClass{"NullDevice"};

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description &options)
{
}

//_____________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(const fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<NullDevice>();
}

//_____________________________________________________________________________
void NullDevice::Bind()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
bool NullDevice::ConditionalRun()
{
    LOG(info) << __PRETTY_FUNCTION__;
    return true;
}

//_____________________________________________________________________________
void NullDevice::Connect()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
void NullDevice::Init()
{
#ifdef NESTDAQ_EXAMPLES_USE_SPDLOG_OTEL
    if (!fLogger) {
        fLogger = std::make_shared<spdlog::logger>(
                      "NullDevice",
                      spdlog::sinks_init_list{nestdaq::telemetry::CreateSpdlogOpenTelemetrySink()});
    }
    fLogger->info("NullDevice example spdlog OTel log");
#endif
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
void NullDevice::InitTask()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
void NullDevice::PostRun()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
void NullDevice::PreRun()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
void NullDevice::Reset()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
void NullDevice::ResetTask()
{
    LOG(info) << __PRETTY_FUNCTION__;
}

//_____________________________________________________________________________
void NullDevice::Run()
{
    LOG(info) << __PRETTY_FUNCTION__;
}
