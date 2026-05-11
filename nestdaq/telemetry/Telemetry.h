#pragma once

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nestdaq::telemetry {

class Attribute {
public:
    Attribute(std::string_view key, std::string_view value)
        : fKey{key}
        , fStringValue{value}
    {
    }

    Attribute(std::string_view key, const char* value)
        : Attribute{key, value == nullptr ? std::string_view{} : std::string_view{value}}
    {
    }

    Attribute(std::string_view key, bool value)
        : fKey{key}
        , fType{NESTDAQ_OTEL_ATTRIBUTE_BOOL}
        , fBoolValue{value ? 1U : 0U}
    {
    }

    template<typename T>
        requires(std::is_integral_v<T> && std::is_signed_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>)
    Attribute(std::string_view key, T value)
        : fKey{key}
        , fType{NESTDAQ_OTEL_ATTRIBUTE_INT64}
        , fIntValue{static_cast<int64_t>(value)}
    {
    }

    template<typename T>
        requires(std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>)
    Attribute(std::string_view key, T value)
        : fKey{key}
        , fType{NESTDAQ_OTEL_ATTRIBUTE_UINT64}
        , fUIntValue{static_cast<uint64_t>(value)}
    {
    }

    template<typename T>
        requires(std::is_floating_point_v<T>)
    Attribute(std::string_view key, T value)
        : fKey{key}
        , fType{NESTDAQ_OTEL_ATTRIBUTE_DOUBLE}
        , fDoubleValue{static_cast<double>(value)}
    {
    }

    auto ToOtelAttribute() const noexcept -> nestdaq_otel_attribute
    {
        return nestdaq_otel_attribute{
            .key = fKey.data(),
            .type = fType,
            .string_value = fStringValue.data(),
            .int_value = fIntValue,
            .uint_value = fUIntValue,
            .double_value = fDoubleValue,
            .bool_value = fBoolValue,
        };
    }

private:
    std::string fKey;
    nestdaq_otel_attribute_type fType{NESTDAQ_OTEL_ATTRIBUTE_STRING};
    std::string fStringValue;
    int64_t fIntValue{0};
    uint64_t fUIntValue{0};
    double fDoubleValue{0.0};
    uint32_t fBoolValue{0};
};

inline auto MakeOtelAttributes(std::span<const Attribute> attributes) -> std::vector<nestdaq_otel_attribute>
{
    auto values = std::vector<nestdaq_otel_attribute>{};
    values.reserve(attributes.size());
    for (const auto& attribute : attributes) {
        values.push_back(attribute.ToOtelAttribute());
    }
    return values;
}

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

    auto SetAttribute(const Attribute& attribute) -> bool
    {
        const auto otelAttribute = attribute.ToOtelAttribute();
        return SetAttribute(otelAttribute);
    }

private:
    TelemetryLibrary* fTelemetry{nullptr};
    uint64_t fHandle{0};
};

class Counter {
public:
    Counter() = default;
    Counter(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description)
        : fLibrary{library}
        , fName{name}
        , fUnit{unit}
        , fDescription{description}
    {
    }

    auto Add(double value, std::initializer_list<Attribute> attributes = {}) const -> bool
    {
        if (fLibrary == nullptr) {
            return true;
        }
        const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
        return fLibrary->MetricAddDoubleCounter(fName, value, fUnit, fDescription, attrs.data(), attrs.size());
    }

private:
    TelemetryLibrary* fLibrary{nullptr};
    std::string fName;
    std::string fUnit;
    std::string fDescription;
};

class Histogram {
public:
    Histogram() = default;
    Histogram(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description)
        : fLibrary{library}
        , fName{name}
        , fUnit{unit}
        , fDescription{description}
    {
    }

    auto Record(double value, std::initializer_list<Attribute> attributes = {}) const -> bool
    {
        if (fLibrary == nullptr) {
            return true;
        }
        const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
        return fLibrary->MetricRecordDoubleHistogram(fName, value, fUnit, fDescription, attrs.data(), attrs.size());
    }

private:
    TelemetryLibrary* fLibrary{nullptr};
    std::string fName;
    std::string fUnit;
    std::string fDescription;
};

class Gauge {
public:
    Gauge() = default;
    Gauge(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description)
        : fLibrary{library}
        , fName{name}
        , fUnit{unit}
        , fDescription{description}
    {
    }

    auto Record(double value, std::initializer_list<Attribute> attributes = {}) const -> bool
    {
        if (fLibrary == nullptr) {
            return true;
        }
        const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
        return fLibrary->MetricRecordDoubleGauge(fName, value, fUnit, fDescription, attrs.data(), attrs.size());
    }

private:
    TelemetryLibrary* fLibrary{nullptr};
    std::string fName;
    std::string fUnit;
    std::string fDescription;
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
    Telemetry() = default;

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

    explicit Telemetry(TelemetryLibrary* library) noexcept
        : fLibrary{library}
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
        if (fLibrary == nullptr) {
            return true;
        }
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
        if (fLibrary == nullptr) {
            return true;
        }
        return fLibrary->MetricRecordDoubleHistogram(name,
                                                     value,
                                                     unit,
                                                     description,
                                                     attributes.data(),
                                                     attributes.size());
    }

    /**
     * @brief Record the latest @p value for a double gauge instrument.
     */
    auto RecordDoubleGauge(std::string_view name,
                           double value,
                           std::string_view unit = "",
                           std::string_view description = "",
                           std::span<const nestdaq_otel_attribute> attributes = {}) -> bool
    {
        if (fLibrary == nullptr) {
            return true;
        }
        return fLibrary->MetricRecordDoubleGauge(name,
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
        if (fLibrary == nullptr) {
            return {};
        }
        return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attributes.data(), attributes.size())};
    }

    auto Counter(std::string_view name,
                 std::string_view unit = "",
                 std::string_view description = "") const -> nestdaq::telemetry::Counter
    {
        return {fLibrary, name, unit, description};
    }

    auto Histogram(std::string_view name,
                   std::string_view unit = "",
                   std::string_view description = "") const -> nestdaq::telemetry::Histogram
    {
        return {fLibrary, name, unit, description};
    }

    auto Gauge(std::string_view name,
               std::string_view unit = "",
               std::string_view description = "") const -> nestdaq::telemetry::Gauge
    {
        return {fLibrary, name, unit, description};
    }

    auto StartSpan(std::string_view name,
                   std::initializer_list<Attribute> attributes) -> TelemetrySpan
    {
        if (fLibrary == nullptr) {
            return {};
        }
        const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
        return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attrs.data(), attrs.size())};
    }

private:
    TelemetryLibrary* fLibrary{nullptr};
};

inline auto ActiveTelemetryLibrary() noexcept -> std::atomic<TelemetryLibrary*>&
{
    static auto value = std::atomic<TelemetryLibrary*>{nullptr};
    return value;
}

inline auto SetActiveTelemetryLibrary(TelemetryLibrary* library) noexcept -> void
{
    ActiveTelemetryLibrary().store(library, std::memory_order_release);
}

inline auto GetTelemetry() noexcept -> Telemetry
{
    return Telemetry{ActiveTelemetryLibrary().load(std::memory_order_acquire)};
}

} // namespace nestdaq::telemetry
