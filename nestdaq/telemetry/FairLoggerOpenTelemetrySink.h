#pragma once

#include <stdint.h>

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

    static auto GetMinSeverity() noexcept -> int32_t;
    static auto Initialize() -> void;
    static auto SetMinSeverity(int32_t severity) noexcept -> void;
    static auto Shutdown() noexcept -> void;
};

} // namespace nestdaq
