#pragma once

#include <memory>
#include <string_view>

namespace spdlog {
class logger;
} // namespace spdlog

namespace nestdaq::telemetry {

/**
 * @brief Create a spdlog logger for NestDAQ examples and user devices.
 *
 * When OTel log export is active and the loaded telemetry plugin provides the
 * optional spdlog sink, the logger exports through OTel. Otherwise it falls
 * back to spdlog's console sink.
 */
auto CreateSpdlogLogger(std::string_view name) -> std::shared_ptr<spdlog::logger>;

} // namespace nestdaq::telemetry
