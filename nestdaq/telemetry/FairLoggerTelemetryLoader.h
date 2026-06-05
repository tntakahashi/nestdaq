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
    std::string nestdaqInstanceId;
    std::string nestdaqInstanceIdStatus{"unresolved"};
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

/**
 * @brief Result of converting a severity name into a FairLogger severity value.
 */
struct SeverityParseResult {
    int32_t value{};
    bool usedFallback{false};
};

/**
 * @brief Add command-line options that configure the optional telemetry plugin.
 */
auto AddTelemetryOptions(boost::program_options::options_description& options,
                         std::string_view defaultServiceName = "nestdaq") -> void;
/**
 * @brief Apply `NESTDAQ_OTEL_*` environment variables to @p options.
 */
auto ApplyEnvironment(TelemetryOptions& options) -> void;
/**
 * @brief Assign one parsed command-line, FairMQ, or environment option.
 */
auto AssignOption(TelemetryOptions& options,
                  std::string_view key,
                  std::string_view value) -> void;
/** @brief Return the final path component of an executable path. */
auto Basename(std::string_view path) -> std::string_view;
/** @brief Read an environment variable as a nullable borrowed C string. */
auto Env(const char* name) -> const char*;
/** @brief Generate and store a service instance id when the user did not set one. */
auto EnsureServiceInstanceId(TelemetryOptions& options) -> void;
/** @brief Generate a UUID string for the default service instance id. */
auto GenerateUuidString() -> std::string;
/**
 * @brief Build the C ABI configuration consumed by `libnestdaq_otel.so`.
 *
 * The returned struct contains string pointers borrowed from @p options, so the
 * options object must outlive the immediate `TelemetryLibrary::InitializeWith()`
 * call that consumes the config.
 */
auto MakeConfig(const TelemetryOptions& options) -> nestdaq_otel_config;
/**
 * @brief Build one signal-specific C ABI exporter configuration.
 *
 * The returned struct borrows the supplied string_view storage.
 */
auto MakeSignalConfig(std::string_view protocol,
                      std::string_view endpointHttp,
                      std::string_view endpointGrpc,
                      std::string_view headers,
                      uint32_t otlpHttpJson) -> nestdaq_otel_signal_config;
/** @brief Parse common true values such as `1`, `true`, `on`, and `yes`. */
auto ParseBool(std::string_view value) -> bool;
/**
 * @brief Parse process arguments into telemetry options.
 *
 * This accepts both `--otel-*` options and selected DAQ aliases such as
 * `--service-name` and `--uuid` so telemetry resource attributes match FairMQ
 * device identity by default.
 */
auto ParseTelemetryOptions(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                           std::string_view defaultServiceName = "nestdaq") -> TelemetryOptions;
/** @brief Convert a FairLogger severity name to its numeric value. */
auto ParseFairLoggerSeverity(std::string_view severity) -> SeverityParseResult;
/** @brief Parse an unsigned integer option with a fallback on invalid input. */
auto ParseUInt32(std::string_view value, uint32_t fallback) -> uint32_t;
/**
 * @brief Read telemetry options from a Boost variables_map after option parsing.
 */
auto ReadTelemetryOptions(const boost::program_options::variables_map& vm,
                          std::string_view defaultServiceName) -> TelemetryOptions;
/** @brief Emit a warning when an unknown severity name falls back to info. */
auto WarnUnknownSeverityFallback(std::string_view severity) -> void;
/** @brief Return the numeric FairLogger severity value for @p severity. */
auto SeverityToFairLoggerValue(std::string_view severity) -> int32_t;
/**
 * @brief Mirror a generated telemetry UUID into FairMQ ProgOptions.
 *
 * Explicit user-provided FairMQ UUID values are preserved.
 */
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

    /** @brief Return the last loader or plugin error message. */
    auto GetLastError() const -> const std::string&;
    /**
     * @brief Initialize the loaded plugin with a C ABI config.
     *
     * @p config may contain borrowed pointers because the plugin copies the
     * values it needs during initialization.
     */
    auto InitializeWith(const nestdaq_otel_config& config) -> bool;
    /** @brief Force-flush initialized telemetry providers. */
    auto ForceFlush(uint64_t timeoutMs) -> bool;
    /** @brief Record a FairMQ state transition as a framework metric sample. */
    auto RecordFrameworkFairMQState(int64_t stateId, std::string_view stateName) -> void;
    /** @brief Add to a user double counter through the plugin C ABI. */
    auto MetricAddDoubleCounter(std::string_view name,
                                double value,
                                std::string_view unit = "",
                                std::string_view description = "",
                                const nestdaq_otel_attribute* attributes = nullptr,
                                uint64_t attributeCount = 0) -> bool;
    /** @brief Record a user double histogram value through the plugin C ABI. */
    auto MetricRecordDoubleHistogram(std::string_view name,
                                     double value,
                                     std::string_view unit = "",
                                     std::string_view description = "",
                                     const nestdaq_otel_attribute* attributes = nullptr,
                                     uint64_t attributeCount = 0) -> bool;
    /** @brief Record a user double gauge value through the plugin C ABI. */
    auto MetricRecordDoubleGauge(std::string_view name,
                                 double value,
                                 std::string_view unit = "",
                                 std::string_view description = "",
                                 const nestdaq_otel_attribute* attributes = nullptr,
                                 uint64_t attributeCount = 0) -> bool;
    /**
     * @brief Load a telemetry plugin shared library and resolve its C ABI.
     *
     * The library is optional in normal NestDAQ startup; callers decide whether
     * a failed load is fatal based on their runtime options.
     */
    auto Load(const std::string& library) -> bool;
    /** @brief End a span handle previously returned by @ref SpanStart. */
    auto SpanEnd(uint64_t spanHandle) -> bool;
    /** @brief Set one attribute on an active span handle. */
    auto SpanSetAttribute(uint64_t spanHandle, const nestdaq_otel_attribute& attribute) -> bool;
    /** @brief Update the NestDAQ instance id attached to exported log records. */
    auto SetNestdaqInstanceId(std::string_view instanceId) -> bool;
    /** @brief Start a span and return its opaque plugin-owned handle. */
    auto SpanStart(std::string_view name,
                   const nestdaq_otel_attribute* attributes = nullptr,
                   uint64_t attributeCount = 0) -> uint64_t;
    /** @brief Update the FairLogger severity threshold by severity name. */
    auto SetMinSeverity(std::string_view severity) -> bool;
    /** @brief Update the FairLogger severity threshold by numeric value. */
    auto SetMinSeverity(int32_t severity) -> bool;
    /** @brief Shut down the plugin once; subsequent calls are no-ops. */
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

/**
 * @brief Subscribe to FairMQ property changes that affect telemetry runtime state.
 */
auto SubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config,
                                     TelemetryLibrary& telemetry) -> void;
/** @brief Remove the telemetry property-change subscription from FairMQ options. */
auto UnsubscribeTelemetryOptionChanges(const fair::mq::ProgOptions& config) -> void;

} // namespace nestdaq::telemetry
