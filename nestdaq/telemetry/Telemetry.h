#pragma once

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace nestdaq::telemetry {

/**
 * @brief Movable RAII wrapper for a span handle owned by the telemetry plugin.
 *
 * A default-constructed or disabled span is inactive. Destroying an active span
 * calls `End()` exactly once. This wrapper intentionally exposes no
 * OpenTelemetry C++ types so executables can avoid linking OpenTelemetry.
 */
class TelemetrySpan {
public:
    TelemetrySpan() = default;
    TelemetrySpan(TelemetryLibrary& telemetry, uint64_t handle) noexcept
        : fTelemetry{&telemetry}
        , fHandle{handle}
    {
    }
    TelemetrySpan(const TelemetrySpan&) = delete;
    auto operator=(const TelemetrySpan&) -> TelemetrySpan& = delete;
    TelemetrySpan(TelemetrySpan&& other) noexcept
        : fTelemetry{other.fTelemetry}
        , fHandle{other.fHandle}
    {
        other.fTelemetry = nullptr;
        other.fHandle = 0;
    }
    auto operator=(TelemetrySpan&& other) noexcept -> TelemetrySpan&
    {
        if (this != &other) {
            End();
            fTelemetry = other.fTelemetry;
            fHandle = other.fHandle;
            other.fTelemetry = nullptr;
            other.fHandle = 0;
        }
        return *this;
    }
    ~TelemetrySpan()
    {
        End();
    }

    auto End() noexcept -> void
    {
        if (fTelemetry != nullptr && fHandle != 0) {
            fTelemetry->SpanEnd(fHandle);
            fHandle = 0;
        }
    }

    auto SetAttribute(const nestdaq_otel_attribute& attribute) -> bool
    {
        return fTelemetry != nullptr && fHandle != 0 && fTelemetry->SpanSetAttribute(fHandle, attribute);
    }

private:
    TelemetryLibrary* fTelemetry{nullptr};
    uint64_t fHandle{0};
};

/**
 * @brief Thin metrics/traces facade over a loaded @ref TelemetryLibrary.
 *
 * The wrapper forwards calls through the runtime-loaded C ABI. Metrics and
 * traces disabled in the active configuration are treated as no-op operations by
 * the plugin where possible.
 */
class Telemetry {
public:
    /**
     * @brief Bind the facade to a loaded telemetry library.
     *
     * The caller must keep @p library alive longer than this facade and any
     * spans created from it.
     */
    explicit Telemetry(TelemetryLibrary& library) noexcept
        : fLibrary{&library}
    {
    }

    /**
     * @brief Add @p value to a double counter instrument.
     */
    auto AddDoubleCounter(std::string_view name,
                          double value,
                          std::string_view unit = "",
                          std::string_view description = "",
                          std::span<const nestdaq_otel_attribute> attributes = {}) -> bool
    {
        return fLibrary->MetricAddDoubleCounter(name,
                                                value,
                                                unit,
                                                description,
                                                attributes.data(),
                                                attributes.size());
    }

    /**
     * @brief Record @p value in a double histogram instrument.
     */
    auto RecordDoubleHistogram(std::string_view name,
                               double value,
                               std::string_view unit = "",
                               std::string_view description = "",
                               std::span<const nestdaq_otel_attribute> attributes = {}) -> bool
    {
        return fLibrary->MetricRecordDoubleHistogram(name,
                                                     value,
                                                     unit,
                                                     description,
                                                     attributes.data(),
                                                     attributes.size());
    }

    /**
     * @brief Start a span.
     *
     * The returned span is inactive when tracing is disabled or span creation
     * fails.
     */
    auto StartSpan(std::string_view name,
                   std::span<const nestdaq_otel_attribute> attributes = {}) -> TelemetrySpan
    {
        return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attributes.data(), attributes.size())};
    }

private:
    TelemetryLibrary* fLibrary;
};

} // namespace nestdaq::telemetry
