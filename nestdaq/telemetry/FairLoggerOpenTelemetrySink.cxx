#include "nestdaq/telemetry/FairLoggerOpenTelemetrySink.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

#include <fairlogger/Logger.h>

#include <opentelemetry/common/timestamp.h>
#include <opentelemetry/logs/log_record.h>
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/severity.h>
#include <opentelemetry/nostd/string_view.h>

#if __has_include("nestdaq/version.h")
#  include "nestdaq/version.h"
#else
static constexpr std::string_view NESTDAQ_VERSION {"unknown"};
#endif

namespace nestdaq {
namespace {

constexpr std::string_view kLoggerName{"FairLogger"};
constexpr std::string_view kLibraryName{"NestDAQ"};
constexpr std::string_view kSchemaUrl;
constexpr std::string_view kSinkKey{"nestdaq-otel-log-sink"};

auto MinSeverity() -> std::atomic<int32_t>&
{
    static std::atomic<int32_t> value{static_cast<int32_t>(fair::Severity::trace)};
    return value;
}

auto SinkRegistered() -> std::atomic<bool>&
{
    static std::atomic<bool> value{false};
    return value;
}

auto ToStringView(std::string_view value) noexcept -> opentelemetry::nostd::string_view
{
    return opentelemetry::nostd::string_view(value.data(), value.size());
}

auto ParseLine(std::string_view line) noexcept -> int64_t
{
    int64_t value = 0;
    const auto *first = line.data();
    const auto *last = line.data() + line.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return 0;
    }
    return value;
}

auto ConvertSeverity(fair::Severity severity) noexcept -> opentelemetry::logs::Severity
{
    switch (severity) {
    case fair::Severity::fatal:
        return opentelemetry::logs::Severity::kFatal4;
    case fair::Severity::critical:
        return opentelemetry::logs::Severity::kFatal;
    case fair::Severity::error:
        return opentelemetry::logs::Severity::kError;
    case fair::Severity::alarm:
        return opentelemetry::logs::Severity::kWarn3;
    case fair::Severity::important:
        return opentelemetry::logs::Severity::kWarn2;
    case fair::Severity::warn:
        return opentelemetry::logs::Severity::kWarn;
    case fair::Severity::state:
        return opentelemetry::logs::Severity::kInfo2;
    case fair::Severity::info:
        return opentelemetry::logs::Severity::kInfo;
    case fair::Severity::detail:
    case fair::Severity::debug:
        return opentelemetry::logs::Severity::kDebug4;
    case fair::Severity::debug1:
        return opentelemetry::logs::Severity::kDebug3;
    case fair::Severity::debug2:
        return opentelemetry::logs::Severity::kDebug2;
    case fair::Severity::debug3:
    case fair::Severity::debug4:
        return opentelemetry::logs::Severity::kDebug;
    case fair::Severity::trace:
        return opentelemetry::logs::Severity::kTrace;
    case fair::Severity::nolog:
    default:
        return opentelemetry::logs::Severity::kInvalid;
    }
}

auto ShouldEmit(fair::Severity severity) noexcept -> bool
{
    const auto minSeverity = static_cast<fair::Severity>(MinSeverity().load(std::memory_order_relaxed));
    return minSeverity != fair::Severity::nolog && severity >= minSeverity;
}

auto EmitLogRecord(const std::string &content, const fair::LogMetaData &metadata) noexcept -> void
{
    if (!ShouldEmit(metadata.severity)) {
        return;
    }

    try {
        auto provider = opentelemetry::logs::Provider::GetLoggerProvider();
        auto logger = provider->GetLogger(ToStringView(kLoggerName),
                                          ToStringView(kLibraryName),
                                          ToStringView(NESTDAQ_VERSION),
                                          ToStringView(kSchemaUrl));
        auto logRecord = logger->CreateLogRecord();
        if (!logRecord) {
            return;
        }

        const auto timestamp = std::chrono::system_clock::time_point{
            std::chrono::seconds{metadata.timestamp} + metadata.us};
        logRecord->SetTimestamp(opentelemetry::common::SystemTimestamp{timestamp});
        logRecord->SetObservedTimestamp(opentelemetry::common::SystemTimestamp{std::chrono::system_clock::now()});
        logRecord->SetSeverity(ConvertSeverity(metadata.severity));
        logRecord->SetBody(ToStringView(content));

        if (!metadata.severity_name.empty()) {
            logRecord->SetAttribute("log.severity.text", ToStringView(metadata.severity_name));
        }
        if (!metadata.process_name.empty()) {
            logRecord->SetAttribute("process.name", ToStringView(metadata.process_name));
        }
        if (!metadata.file.empty()) {
            logRecord->SetAttribute("code.file.path", ToStringView(metadata.file));
        }
        const auto line = ParseLine(metadata.line);
        if (line > 0) {
            logRecord->SetAttribute("code.line.number", line);
        }
        if (!metadata.func.empty()) {
            logRecord->SetAttribute("code.function.name", ToStringView(metadata.func));
        }
        logRecord->SetAttribute("thread.id",
                                static_cast<int64_t>(std::hash<std::thread::id> {}(std::this_thread::get_id())));

        logger->EmitLogRecord(std::move(logRecord));
    } catch (const std::exception &ex) {
        std::cerr << "FairLoggerOpenTelemetrySink: failed to emit log record: " << ex.what() << '\n';
    } catch (...) {
        std::cerr << "FairLoggerOpenTelemetrySink: failed to emit log record\n";
    }
}

} // namespace

auto FairLoggerOpenTelemetrySink::Initialize() -> void
{
    bool expected = false;
    if (!SinkRegistered().compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    try {
        fair::Logger::AddCustomSink(std::string{kSinkKey},
                                    fair::Severity::trace,
        [](const std::string &content, const fair::LogMetaData &metadata) {
            EmitLogRecord(content, metadata);
        });
    } catch (...) {
        SinkRegistered().store(false, std::memory_order_release);
        throw;
    }
}

auto FairLoggerOpenTelemetrySink::Shutdown() noexcept -> void
{
    bool expected = true;
    if (!SinkRegistered().compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
        return;
    }

    try {
        fair::Logger::RemoveCustomSink(std::string{kSinkKey});
    } catch (const std::exception &ex) {
        std::cerr << "FairLoggerOpenTelemetrySink: failed to remove sink: " << ex.what() << '\n';
    } catch (...) {
        std::cerr << "FairLoggerOpenTelemetrySink: failed to remove sink\n";
    }
}

auto FairLoggerOpenTelemetrySink::SetMinSeverity(int32_t severity) noexcept -> void
{
    MinSeverity().store(severity, std::memory_order_release);
}

auto FairLoggerOpenTelemetrySink::GetMinSeverity() noexcept -> int32_t
{
    return MinSeverity().load(std::memory_order_acquire);
}

} // namespace nestdaq
