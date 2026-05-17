#pragma once

/**
 * @file TimeUtil.h
 * @brief Helpers for converting steady-clock updates into wall-clock timestamps.
 */

#include <chrono>
#include <string>
#include <utility>

namespace daq::service {

/** @brief Format a system-clock time point for Redis timestamp fields. */
const std::string to_date(const std::chrono::system_clock::time_point &p);
/**
 * @brief Convert a steady-clock update time to wall-clock time.
 *
 * The returned pair contains nanoseconds since epoch and the derived
 * system-clock timestamp corresponding to @p t.
 */
auto update_date(const std::chrono::system_clock::time_point &s,
                 const std::chrono::steady_clock::time_point &t)
-> const std::pair<std::chrono::nanoseconds, std::chrono::system_clock::time_point>;

} // namespace daq::service
