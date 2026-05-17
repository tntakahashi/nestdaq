#pragma once

#include <nestdaq/telemetry/OpenTelemetryInitializer.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace boost::program_options {
class options_description;
class variables_map;
} // namespace boost::program_options

namespace fair::mq {
class ProgOptions;
} // namespace fair::mq

namespace nestdaq::telemetry {

/** Default soname loaded by NestDAQ executables when telemetry is enabled. */
inline constexpr std::string_view kDefaultTelemetryLibrary{"libnestdaq_otel.so"};
inline constexpr std::string_view kDefaultProtocol{"console"};
inline constexpr std::string_view kDefaultLogHttpEndpoint{"http://localhost:4318/v1/logs"};
inline constexpr std::string_view kDefaultMetricHttpEndpoint{"http://localhost:4318/v1/metrics"};
inline constexpr std::string_view kDefaultTraceHttpEndpoint{"http://localhost:4318/v1/traces"};
inline constexpr std::string_view kDefaultGrpcEndpoint{"localhost:4317"};
inline constexpr uint32_t kDefaultTimeoutMs{5000};
inline constexpr uint32_t kDefaultMetricExportIntervalMs{1000};
inline constexpr std::string_view kTelemetryConfigSubscriber{"nestdaq-telemetry"};
inline constexpr std::string_view kDefaultServiceNamespace{"nestdaq"};

/**
 * @brief Runtime options used to configure the telemetry plugin.
 *
 * Values are initialized from defaults, optionally overridden by environment
 * variables and command-line/FairMQ options. `MakeConfig()` returns borrowed
 * pointers into this object, so the object must outlive the call to
 * `TelemetryLibrary::InitializeWith()`.
 */
struct TelemetryOptions {
    std::string library{kDefaultTelemetryLibrary};
    std::string logProtocol{kDefaultProtocol};
    std::string metricProtocol;
    std::string traceProtocol;
    std::string logEndpointHttp{kDefaultLogHttpEndpoint};
    std::string logEndpointGrpc{kDefaultGrpcEndpoint};
    std::string metricEndpointHttp{kDefaultMetricHttpEndpoint};
    std::string metricEndpointGrpc{kDefaultGrpcEndpoint};
    std::string traceEndpointHttp{kDefaultTraceHttpEndpoint};
    std::string traceEndpointGrpc{kDefaultGrpcEndpoint};
    std::string logHeaders;
    std::string metricHeaders;
    std::string traceHeaders;
    std::string severity{"info"};
    std::string serviceName{"nestdaq"};
    std::string serviceNamespace{kDefaultServiceNamespace};
    std::string serviceInstanceId;
    std::string fairmqId;
    std::string fairmqDevice;
    std::string fairmqSession;
    std::string fairmqTransport;
    uint32_t timeoutMs{kDefaultTimeoutMs};
    uint32_t metricExportIntervalMs{kDefaultMetricExportIntervalMs};
    uint32_t logOtlpHttpJson{1};
    uint32_t metricOtlpHttpJson{1};
    uint32_t traceOtlpHttpJson{1};
    bool required{false};
    bool generatedServiceInstanceId{false};
};

struct SeverityParseResult {
    int32_t value{};
    bool usedFallback{false};
};

auto AddTelemetryOptions(boost::program_options::options_description& options,
                         std::string_view defaultServiceName = "nestdaq") -> void;
auto ApplyEnvironment(TelemetryOptions& options) -> void;
auto AssignOption(TelemetryOptions& options,
                  std::string_view key,
                  std::string_view value) -> void;
auto Basename(std::string_view path) -> std::string_view;
auto Env(const char* name) -> const char*;
auto EnsureServiceInstanceId(TelemetryOptions& options) -> void;
auto GenerateUuidString() -> std::string;
auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config;
auto MakeSignalConfig(std::string_view protocol,
                      std::string_view endpointHttp,
                      std::string_view endpointGrpc,
                      std::string_view headers,
                      uint32_t otlpHttpJson) -> nestdaq_otel_signal_config;
auto ParseBool(std::string_view value) -> bool;
auto ParseTelemetryOptions(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                           std::string_view defaultServiceName = "nestdaq") -> TelemetryOptions;
auto ParseFairLoggerSeverity(std::string_view severity) -> SeverityParseResult;
auto ParseUInt32(std::string_view value, uint32_t fallback) -> uint32_t;
auto ReadTelemetryOptions(const boost::program_options::variables_map& vm,
                          std::string_view defaultServiceName) -> TelemetryOptions;
auto WarnUnknownSeverityFallback(std::string_view severity) -> void;
auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t;
auto SetGeneratedUuidProperty(fair::mq::ProgOptions& config,
                              const TelemetryOptions& options,
                              std::string_view key = "uuid") -> void;

/**
 * @brief Runtime loader for the optional OpenTelemetry plugin.
 *
 * `TelemetryLibrary` owns the `dlopen()` handle and resolves the required
 * `nestdaq_otel_*` C ABI symbols. It is intentionally non-copyable and
 * non-movable because the plugin state is process-wide. Destruction shuts down
 * telemetry once and then closes the shared library.
 */
class TelemetryLibrary {
public:
    TelemetryLibrary() = default;
    TelemetryLibrary(const TelemetryLibrary&) = delete;
    auto operator=(const TelemetryLibrary&) -> TelemetryLibrary& = delete;
    TelemetryLibrary(TelemetryLibrary&&) = delete;
    auto operator=(TelemetryLibrary&&) -> TelemetryLibrary& = delete;
    ~TelemetryLibrary();

    auto GetLastError() const -> const std::string&;
    auto InitializeWith(const nestdaq_otel_config& config) -> bool;
    auto ForceFlush(uint64_t timeoutMs) -> bool;
    auto RecordFrameworkFairMQState(int64_t stateId, std::string_view stateName) -> void;
    auto MetricAddDoubleCounter(std::string_view name,
                                double value,
                                std::string_view unit = "",
                                std::string_view description = "",
                                const nestdaq_otel_attribute* attributes = nullptr,
                                uint64_t attributeCount = 0) -> bool;
    auto MetricRecordDoubleHistogram(std::string_view name,
                                     double value,
                                     std::string_view unit = "",
                                     std::string_view description = "",
                                     const nestdaq_otel_attribute* attributes = nullptr,
                                     uint64_t attributeCount = 0) -> bool;
    auto MetricRecordDoubleGauge(std::string_view name,
                                 double value,
                                 std::string_view unit = "",
                                 std::string_view description = "",
                                 const nestdaq_otel_attribute* attributes = nullptr,
                                 uint64_t attributeCount = 0) -> bool;
    auto Load(const std::string& library) -> bool;
    auto SpanEnd(uint64_t spanHandle) -> bool;
    auto SpanSetAttribute(uint64_t spanHandle, const nestdaq_otel_attribute& attribute) -> bool;
    auto SetNestdaqInstanceId(std::string_view instanceId) -> bool;
    auto SpanStart(std::string_view name,
                   const nestdaq_otel_attribute* attributes = nullptr,
                   uint64_t attributeCount = 0) -> uint64_t;
    auto SetMinSeverity(std::string_view severity) -> bool;
    auto SetMinSeverity(int32_t severity) -> bool;
    auto ShutdownTelemetry(uint64_t timeoutMs) const -> void;

private:
    auto StoreResult(int rc) -> bool;

    void* fHandle{nullptr};
    std::function<int(const nestdaq_otel_config*)> fInitialize;
    std::function<int(uint64_t)> fForceFlush;
    std::function<int(uint64_t)> fShutdown;
    std::function<const char*()> fLastErrorFunction;
    std::function<void(int64_t, const char*)> fRecordFrameworkFairMQState;
    std::function<int(int32_t)> fSetMinSeverity;
    std::function<int(const char*)> fSetNestdaqInstanceId;
    std::function<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)> fMetricAddDoubleCounter;
    std::function<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)> fMetricRecordDoubleHistogram;
    std::function<int(const char*, double, const char*, const char*, const nestdaq_otel_attribute*, uint64_t)> fMetricRecordDoubleGauge;
    std::function<int(uint64_t)> fSpanEnd;
    std::function<int(uint64_t, const nestdaq_otel_attribute*)> fSpanSetAttribute;
    std::function<uint64_t(const char*, const nestdaq_otel_attribute*, uint64_t)> fSpanStart;
    mutable bool fShutdownCalled{false};
    std::string fLastError;
};

auto SubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config,
                                     TelemetryLibrary& telemetry) -> void;
auto UnsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void;

} // namespace nestdaq::telemetry
