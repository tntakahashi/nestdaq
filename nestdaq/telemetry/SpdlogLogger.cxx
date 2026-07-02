/** @file
 *  @brief Implements spdlog logger creation with OTel sink or console fallback.
 */

#include "nestdaq/telemetry/SpdlogLogger.h"

#include "nestdaq/telemetry/Telemetry.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <string>
#include <vector>

namespace nestdaq::telemetry {
namespace {
constexpr std::string_view kConsolePattern{"[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"};
} // namespace

auto CreateSpdlogLogger(std::string_view name) -> std::shared_ptr<spdlog::logger>
{
    auto sinks = std::vector<spdlog::sink_ptr>{};
    if (auto otelSink = CreateActiveSpdlogSink()) {
        sinks.emplace_back(std::move(otelSink));
    } else {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern(std::string{kConsolePattern});
        sinks.emplace_back(std::move(consoleSink));
    }
    return std::make_shared<spdlog::logger>(std::string{name}, sinks.begin(), sinks.end());
}

} // namespace nestdaq::telemetry
