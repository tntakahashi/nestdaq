/** @file
 *  @brief Implements spdlog logger creation with OTel sink or console fallback.
 */

#include "nestdaq/telemetry/SpdlogLogger.h"

#include "nestdaq/telemetry/Telemetry.h"

#include <spdlog/async_logger.h>
#include <spdlog/details/thread_pool.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace nestdaq::telemetry {
namespace {
struct ThreadPoolEntry {
    uint32_t queueSize{};
    uint32_t threadCount{};
    std::shared_ptr<spdlog::details::thread_pool> pool;
};

auto ThreadPoolMutex() -> std::mutex& {
    static auto value = std::mutex{};
    return value;
}

auto ThreadPools() -> std::vector<ThreadPoolEntry>& {
    static auto value = std::vector<ThreadPoolEntry> {};
    return value;
}

auto MakeOverflowPolicy(std::string_view value) -> spdlog::async_overflow_policy {
    if (value == "overrun_oldest") {
        return spdlog::async_overflow_policy::overrun_oldest;
    }
    if (value == "discard_new") {
        return spdlog::async_overflow_policy::discard_new;
    }
    return spdlog::async_overflow_policy::block;
}

auto GetOrCreateThreadPool(const SpdlogAsyncOptions& options) -> std::shared_ptr<spdlog::details::thread_pool> {
    const auto lock = std::scoped_lock{ThreadPoolMutex()};
    for (const auto& entry : ThreadPools()) {
        if (entry.queueSize == options.queueSize && entry.threadCount == options.threadCount) {
            return entry.pool;
        }
    }

    auto pool = std::make_shared<spdlog::details::thread_pool>(options.queueSize, options.threadCount);
    ThreadPools().push_back(ThreadPoolEntry{
        .queueSize = options.queueSize,
        .threadCount = options.threadCount,
        .pool = pool,
    });
    return pool;
}
} // namespace

auto CreateSpdlogLogger(std::string_view name) -> std::shared_ptr<spdlog::logger> {
    auto sinks = std::vector<spdlog::sink_ptr> {};
    if (GetSpdlogNativeConsoleEnabled()) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern(GetSpdlogConsolePattern());
        sinks.emplace_back(std::move(consoleSink));
    }
    if (auto otelSink = CreateActiveSpdlogSink()) {
        sinks.emplace_back(std::move(otelSink));
    }
    const auto asyncOptions = GetSpdlogAsyncOptions();
    if (asyncOptions.enabled) {
        return std::make_shared<spdlog::async_logger>(
                   std::string{name},
                   sinks.begin(),
                   sinks.end(),
                   GetOrCreateThreadPool(asyncOptions),
                   MakeOverflowPolicy(asyncOptions.overflowPolicy));
    }
    return std::make_shared<spdlog::logger>(std::string{name}, sinks.begin(), sinks.end());
}

} // namespace nestdaq::telemetry
