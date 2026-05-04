#pragma once

#include <stdint.h>

namespace nestdaq {

class FairLoggerOpenTelemetrySink {
public:
    FairLoggerOpenTelemetrySink() = delete;

    static auto GetMinSeverity() noexcept -> int32_t;
    static auto Initialize() -> void;
    static auto SetMinSeverity(int32_t severity) noexcept -> void;
    static auto Shutdown() noexcept -> void;
};

} // namespace nestdaq
