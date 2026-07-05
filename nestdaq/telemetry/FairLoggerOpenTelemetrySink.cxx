/** @file
 *  @brief Implements the FairLogger sink that forwards log records to OpenTelemetry.
 */

#include "nestdaq/telemetry/FairLoggerOpenTelemetrySink.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#ifdef __linux__
#  include <sys/syscall.h>
#  include <unistd.h>
#endif

#include <fairlogger/Logger.h>
#include <fairlogger/Version.h>

#include <opentelemetry/common/timestamp.h>
#include <opentelemetry/logs/log_record.h>
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/severity.h>
#include <opentelemetry/nostd/string_view.h>
#include <opentelemetry/semconv/code_attributes.h>
#include <opentelemetry/semconv/incubating/thread_attributes.h>

#include "nestdaq/telemetry/Compat.h"
#include "nestdaq/telemetry/FairMQThroughputLogParser.h"
#include "nestdaq/telemetry/OpenTelemetryInitializer.h"

namespace nestdaq {
namespace {

constexpr std::string_view kLoggerName{"FairLogger"};
constexpr std::string_view kLibraryName{"FairLogger"};
constexpr std::string_view kLibraryVersion{FAIRLOGGER_VERSION};
constexpr std::string_view kSchemaUrl;
constexpr std::string_view kSinkKey{"nestdaq-otel-log-sink"};
constexpr auto kSeverityMap = std::array{
    opentelemetry::logs::Severity::kInvalid, // nolog
    opentelemetry::logs::Severity::kTrace,
    opentelemetry::logs::Severity::kTrace2,
    opentelemetry::logs::Severity::kTrace2,
    opentelemetry::logs::Severity::kTrace3,
    opentelemetry::logs::Severity::kTrace4,
    opentelemetry::logs::Severity::kDebug,
    opentelemetry::logs::Severity::kDebug2,
    opentelemetry::logs::Severity::kInfo,
    opentelemetry::logs::Severity::kInfo2,
    opentelemetry::logs::Severity::kWarn,
    opentelemetry::logs::Severity::kWarn2,
    opentelemetry::logs::Severity::kWarn3,
    opentelemetry::logs::Severity::kError,
    opentelemetry::logs::Severity::kError2,
    opentelemetry::logs::Severity::kFatal,
};

constexpr auto convertSeverity(fair::Severity severity) noexcept -> opentelemetry::logs::Severity;
auto currentThreadId() noexcept -> uint64_t;
auto emitLogRecord(const std::string &content, const fair::LogMetaData &metadata) noexcept -> void;
auto fairLoggerSeverityName(fair::Severity severity) noexcept -> std::string_view;
auto instanceIdStorage() -> std::string &;
auto instanceIdMutex() -> std::mutex &;
auto minSeverity() -> std::atomic<int32_t>&;
auto parseInstanceIndex(std::string_view instance_id) -> std::optional<std::pair<std::string_view, int64_t>>;
auto parseLine(std::string_view line) -> int64_t;
auto shouldEmit(fair::Severity severity) noexcept -> bool;
auto sinkRegistered() -> std::atomic<bool>&;
auto toStringView(std::string_view value) noexcept -> opentelemetry::nostd::string_view;

constexpr auto convertSeverity(fair::Severity severity) noexcept -> opentelemetry::logs::Severity
{
    using opentelemetry::logs::Severity;
    const auto value = static_cast<int32_t>(severity);
    if (value < 0) {
        return Severity::kInvalid;
    }
    const auto index = static_cast<size_t>(value);
    if (index >= kSeverityMap.size()) {
        return Severity::kInvalid;
    }
    return kSeverityMap.at(index);
}

static_assert(kSeverityMap.size() == fair::Logger::fSeverityNames.size());
static_assert(convertSeverity(fair::Severity::nolog) == opentelemetry::logs::Severity::kInvalid);
static_assert(convertSeverity(fair::Severity::trace) == opentelemetry::logs::Severity::kTrace);
static_assert(convertSeverity(fair::Severity::debug4) == opentelemetry::logs::Severity::kTrace2);
static_assert(convertSeverity(fair::Severity::debug3) == opentelemetry::logs::Severity::kTrace2);
static_assert(convertSeverity(fair::Severity::debug2) == opentelemetry::logs::Severity::kTrace3);
static_assert(convertSeverity(fair::Severity::debug1) == opentelemetry::logs::Severity::kTrace4);
static_assert(convertSeverity(fair::Severity::debug) == opentelemetry::logs::Severity::kDebug);
static_assert(convertSeverity(fair::Severity::detail) == opentelemetry::logs::Severity::kDebug2);
static_assert(convertSeverity(fair::Severity::info) == opentelemetry::logs::Severity::kInfo);
static_assert(convertSeverity(fair::Severity::state) == opentelemetry::logs::Severity::kInfo2);
static_assert(convertSeverity(fair::Severity::warn) == opentelemetry::logs::Severity::kWarn);
static_assert(convertSeverity(fair::Severity::important) == opentelemetry::logs::Severity::kWarn2);
static_assert(convertSeverity(fair::Severity::alarm) == opentelemetry::logs::Severity::kWarn3);
static_assert(convertSeverity(fair::Severity::error) == opentelemetry::logs::Severity::kError);
static_assert(convertSeverity(fair::Severity::critical) == opentelemetry::logs::Severity::kError2);
static_assert(convertSeverity(fair::Severity::fatal) == opentelemetry::logs::Severity::kFatal);
static_assert(convertSeverity(static_cast<fair::Severity>(-1)) == opentelemetry::logs::Severity::kInvalid);
static_assert(convertSeverity(static_cast<fair::Severity>(fair::Logger::fSeverityNames.size())) ==
              opentelemetry::logs::Severity::kInvalid);

auto currentThreadId() noexcept -> uint64_t
{
#ifdef __linux__
    // Use the native Linux TID instead of std::this_thread::get_id() so logs can be correlated
    // with /proc, top -H, debuggers, and profilers. This matches spdlog's Linux thread id behavior.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    static thread_local const auto tid = static_cast<uint64_t>(::syscall(SYS_gettid));
    return tid;
#else
    static thread_local const auto tid =
        static_cast<uint64_t>(std::hash<std::thread::id> {}(std::this_thread::get_id()));
    return tid;
#endif
}

auto emitLogRecord(const std::string &content, const fair::LogMetaData &metadata) noexcept -> void
{
    if (auto sample = telemetry::parseFairMQThroughputLog(content)) {
        OpenTelemetryInitializer::RecordFairMQThroughput(*sample);
    }

    if (!shouldEmit(metadata.severity)) {
        return;
    }

    try {
        auto provider = opentelemetry::logs::Provider::GetLoggerProvider();
        auto logger = provider->GetLogger(toStringView(kLoggerName),
                                          toStringView(kLibraryName),
                                          toStringView(kLibraryVersion),
                                          toStringView(kSchemaUrl));
        auto log_record = logger->CreateLogRecord();
        if (!log_record) {
            return;
        }

        const auto timestamp = std::chrono::system_clock::time_point{
            std::chrono::seconds{metadata.timestamp} + metadata.us};
        log_record->SetTimestamp(opentelemetry::common::SystemTimestamp{timestamp});
        log_record->SetObservedTimestamp(opentelemetry::common::SystemTimestamp{std::chrono::system_clock::now()});
        // SetSeverity records the OpenTelemetry-defined SeverityNumber and SeverityText.
        // The FairLogger original level is kept below as fairlogger.severity.* attributes.
        log_record->SetSeverity(convertSeverity(metadata.severity));
        log_record->SetBody(toStringView(content));

        auto severity_name = std::string_view{metadata.severity_name};
        if (severity_name.empty()) {
            severity_name = fairLoggerSeverityName(metadata.severity);
        }
        log_record->SetAttribute("fairlogger.severity.number", static_cast<int64_t>(metadata.severity));
        if (!severity_name.empty()) {
            log_record->SetAttribute("fairlogger.severity.text", toStringView(severity_name));
        }
        auto instance_id = std::string{};
        {
            std::scoped_lock lock{instanceIdMutex()};
            instance_id = instanceIdStorage();
        }
        if (!instance_id.empty()) {
            log_record->SetAttribute("nestdaq.instance.id", toStringView(instance_id));
            if (const auto parsed = parseInstanceIndex(instance_id)) {
                log_record->SetAttribute("nestdaq.instance.name", toStringView(parsed->first));
                log_record->SetAttribute("nestdaq.instance.index", parsed->second);
            }
        }
        if (!metadata.process_name.empty()) {
            log_record->SetAttribute("process.name", toStringView(metadata.process_name));
        }
        if (!metadata.file.empty()) {
            log_record->SetAttribute(opentelemetry::semconv::code::kCodeFilePath, toStringView(metadata.file));
        }
        const auto line = parseLine(metadata.line);
        if (line > 0) {
            log_record->SetAttribute(opentelemetry::semconv::code::kCodeLineNumber, line);
        }
        if (!metadata.func.empty()) {
            log_record->SetAttribute(opentelemetry::semconv::code::kCodeFunctionName, toStringView(metadata.func));
        }
        log_record->SetAttribute(opentelemetry::semconv::thread::kThreadId, currentThreadId());

        logger->EmitLogRecord(std::move(log_record));
    } catch (const std::exception &ex) {
        std::cerr << "FairLoggerOpenTelemetrySink: failed to emit log record: " << ex.what() << '\n';
    } catch (...) {
        std::cerr << "FairLoggerOpenTelemetrySink: failed to emit log record\n";
    }
}

auto fairLoggerSeverityName(fair::Severity severity) noexcept -> std::string_view
{
    const auto value = static_cast<int32_t>(severity);
    if (value < 0) {
        return {};
    }
    if (static_cast<size_t>(value) >= fair::Logger::fSeverityNames.size()) {
        return {};
    }
    return fair::Logger::SeverityName(severity);
}

auto instanceIdStorage() -> std::string &
{
    static auto value = std::string{};
    return value;
}

auto instanceIdMutex() -> std::mutex &
{
    static auto value = std::mutex{};
    return value;
}

auto minSeverity() -> std::atomic<int32_t>&
{
    static std::atomic<int32_t> value{static_cast<int32_t>(fair::Severity::trace)};
    return value;
}

auto parseInstanceIndex(std::string_view instance_id) -> std::optional<std::pair<std::string_view, int64_t>>
{
    const auto separator = instance_id.rfind('-');
    if (separator == std::string_view::npos || separator == 0 || separator + 1 == instance_id.size()) {
        return std::nullopt;
    }

    auto index = int64_t{0};
    const auto suffix = instance_id.substr(separator + 1);
    if (!telemetry::compat::parseInteger(suffix, index)) {
        return std::nullopt;
    }
    return std::pair{instance_id.substr(0, separator), index};
}

auto parseLine(std::string_view line) -> int64_t
{
    int64_t value = 0;
    if (!telemetry::compat::parseInteger(line, value)) {
        return 0;
    }
    return value;
}

auto shouldEmit(fair::Severity severity) noexcept -> bool
{
    const auto min_severity = static_cast<fair::Severity>(minSeverity().load(std::memory_order_relaxed));
    return min_severity != fair::Severity::nolog && severity >= min_severity;
}

auto sinkRegistered() -> std::atomic<bool>&
{
    static std::atomic<bool> value{false};
    return value;
}

auto toStringView(std::string_view value) noexcept -> opentelemetry::nostd::string_view
{
    return {value.data(), value.size()};
}

} // namespace

auto FairLoggerOpenTelemetrySink::getMinSeverity() noexcept -> int32_t
{
    return minSeverity().load(std::memory_order_acquire);
}

auto FairLoggerOpenTelemetrySink::initialize() -> void
{
    bool expected = false;
    if (!sinkRegistered().compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    try {
        fair::Logger::AddCustomSink(std::string{kSinkKey},
                                    fair::Severity::trace,
        [](const std::string &content, const fair::LogMetaData &metadata) {
            emitLogRecord(content, metadata);
        });
    } catch (...) {
        sinkRegistered().store(false, std::memory_order_release);
        throw;
    }
}

auto FairLoggerOpenTelemetrySink::setNestdaqInstanceId(std::string_view instanceId) -> void
{
    std::scoped_lock lock{instanceIdMutex()};
    instanceIdStorage() = instanceId;
}

auto FairLoggerOpenTelemetrySink::setMinSeverity(int32_t severity) noexcept -> void
{
    minSeverity().store(severity, std::memory_order_release);
}

auto FairLoggerOpenTelemetrySink::shutdown() noexcept -> void
{
    setNestdaqInstanceId({});

    bool expected = true;
    if (!sinkRegistered().compare_exchange_strong(expected, false, std::memory_order_acq_rel)) {
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

} // namespace nestdaq
