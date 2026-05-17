#pragma once

#include <stdint.h>
#include <string_view>

namespace nestdaq {

/**
 * @brief FairLogger custom sink that forwards log records to OpenTelemetry.
 *
 * The sink is installed by the telemetry plugin after a logger provider has
 * been configured. The class exposes only lifecycle controls because FairLogger
 * invokes the actual sink callback internally.
 */
class FairLoggerOpenTelemetrySink {
public:
    FairLoggerOpenTelemetrySink() = delete;

    /** @brief Return the current FairLogger severity threshold used by the sink. */
    static auto GetMinSeverity() noexcept -> int32_t;
    /** @brief Install the FairLogger custom sink once for the process. */
    static auto Initialize() -> void;
    /** @brief Set the NestDAQ instance id attached to subsequent log records. */
    static auto SetNestdaqInstanceId(std::string_view instanceId) -> void;
    /** @brief Set the minimum FairLogger severity exported as OpenTelemetry logs. */
    static auto SetMinSeverity(int32_t severity) noexcept -> void;
    /** @brief Remove the custom sink and clear per-process log attributes. */
    static auto Shutdown() noexcept -> void;
};

} // namespace nestdaq
