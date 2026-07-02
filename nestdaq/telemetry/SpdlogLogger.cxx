/** @file
 *  @brief Implements spdlog logger creation with OTel sink or console fallback.
 */

#include "nestdaq/telemetry/SpdlogLogger.h"

#include "nestdaq/telemetry/Telemetry.h"

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nestdaq::telemetry {

auto CreateSpdlogLogger(std::string_view name) -> std::shared_ptr<spdlog::logger>
{
    auto sinks = std::vector<spdlog::sink_ptr>{};
    if (GetSpdlogNativeConsoleEnabled()) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern(GetSpdlogConsolePattern());
        sinks.emplace_back(std::move(consoleSink));
    }
    if (auto otelSink = CreateActiveSpdlogSink()) {
        sinks.emplace_back(std::move(otelSink));
    }
    return std::make_shared<spdlog::logger>(std::string{name}, sinks.begin(), sinks.end());
}

} // namespace nestdaq::telemetry
