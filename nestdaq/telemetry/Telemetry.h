#pragma once

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>

#include <cstdint>
#include <span>
#include <string_view>

namespace nestdaq::telemetry {

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

class Telemetry {
public:
    explicit Telemetry(TelemetryLibrary& library) noexcept
        : fLibrary{&library}
    {
    }

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

    auto StartSpan(std::string_view name,
                   std::span<const nestdaq_otel_attribute> attributes = {}) -> TelemetrySpan
    {
        return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attributes.data(), attributes.size())};
    }

private:
    TelemetryLibrary* fLibrary;
};

} // namespace nestdaq::telemetry
