#pragma once

#include <nestdaq/telemetry/OpenTelemetryInitializer.h>

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace nestdaq::telemetry {

class TelemetryLibrary;

namespace detail {
template<typename T>
concept MetricValue = std::is_arithmetic_v<std::remove_cvref_t<T>> &&
                      !std::is_same_v<std::remove_cvref_t<T>, bool>;
} // namespace detail

class Attribute {
public:
    Attribute(std::string_view key, std::string_view value);
    Attribute(std::string_view key, const char* value);
    Attribute(std::string_view key, bool value);

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

    auto ToOtelAttribute() const noexcept -> nestdaq_otel_attribute;

private:
    std::string fKey;
    nestdaq_otel_attribute_type fType{NESTDAQ_OTEL_ATTRIBUTE_STRING};
    std::string fStringValue;
    int64_t fIntValue{0};
    uint64_t fUIntValue{0};
    double fDoubleValue{0.0};
    uint32_t fBoolValue{0};
};

auto MakeOtelAttributes(std::span<const Attribute> attributes) -> std::vector<nestdaq_otel_attribute>;

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
    TelemetrySpan(TelemetryLibrary& telemetry, uint64_t handle) noexcept;
    TelemetrySpan(const TelemetrySpan&) = delete;
    auto operator=(const TelemetrySpan&) -> TelemetrySpan& = delete;
    TelemetrySpan(TelemetrySpan&& other) noexcept;
    auto operator=(TelemetrySpan&& other) noexcept -> TelemetrySpan&;
    ~TelemetrySpan();

    auto End() noexcept -> void;
    auto SetAttribute(const nestdaq_otel_attribute& attribute) -> bool;
    auto SetAttribute(const Attribute& attribute) -> bool;

private:
    TelemetryLibrary* fTelemetry{nullptr};
    uint64_t fHandle{0};
};

class Counter {
public:
    Counter() = default;
    Counter(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description);

    auto Add(double value, std::initializer_list<Attribute> attributes = {}) const -> bool;

    template<detail::MetricValue T>
    auto Add(T value, std::initializer_list<Attribute> attributes = {}) const -> bool
    {
        return Add(static_cast<double>(value), attributes);
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
    Histogram(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description);

    auto Record(double value, std::initializer_list<Attribute> attributes = {}) const -> bool;

    template<detail::MetricValue T>
    auto Record(T value, std::initializer_list<Attribute> attributes = {}) const -> bool
    {
        return Record(static_cast<double>(value), attributes);
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
    Gauge(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description);

    auto Record(double value, std::initializer_list<Attribute> attributes = {}) const -> bool;

    template<detail::MetricValue T>
    auto Record(T value, std::initializer_list<Attribute> attributes = {}) const -> bool
    {
        return Record(static_cast<double>(value), attributes);
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
    explicit Telemetry(TelemetryLibrary& library) noexcept;
    explicit Telemetry(TelemetryLibrary* library) noexcept;

    auto AddDoubleCounter(std::string_view name,
                          double value,
                          std::string_view unit = "",
                          std::string_view description = "",
                          std::span<const nestdaq_otel_attribute> attributes = {}) -> bool;

    template<detail::MetricValue T>
    auto AddCounter(std::string_view name,
                    T value,
                    std::string_view unit = "",
                    std::string_view description = "",
                    std::span<const nestdaq_otel_attribute> attributes = {}) -> bool
    {
        return AddDoubleCounter(name, static_cast<double>(value), unit, description, attributes);
    }

    auto RecordDoubleHistogram(std::string_view name,
                               double value,
                               std::string_view unit = "",
                               std::string_view description = "",
                               std::span<const nestdaq_otel_attribute> attributes = {}) -> bool;

    template<detail::MetricValue T>
    auto RecordHistogram(std::string_view name,
                         T value,
                         std::string_view unit = "",
                         std::string_view description = "",
                         std::span<const nestdaq_otel_attribute> attributes = {}) -> bool
    {
        return RecordDoubleHistogram(name, static_cast<double>(value), unit, description, attributes);
    }

    auto RecordDoubleGauge(std::string_view name,
                           double value,
                           std::string_view unit = "",
                           std::string_view description = "",
                           std::span<const nestdaq_otel_attribute> attributes = {}) -> bool;

    template<detail::MetricValue T>
    auto RecordGauge(std::string_view name,
                     T value,
                     std::string_view unit = "",
                     std::string_view description = "",
                     std::span<const nestdaq_otel_attribute> attributes = {}) -> bool
    {
        return RecordDoubleGauge(name, static_cast<double>(value), unit, description, attributes);
    }

    auto StartSpan(std::string_view name,
                   std::span<const nestdaq_otel_attribute> attributes = {}) -> TelemetrySpan;

    auto Counter(std::string_view name,
                 std::string_view unit = "",
                 std::string_view description = "") const -> nestdaq::telemetry::Counter;
    auto Histogram(std::string_view name,
                   std::string_view unit = "",
                   std::string_view description = "") const -> nestdaq::telemetry::Histogram;
    auto Gauge(std::string_view name,
               std::string_view unit = "",
               std::string_view description = "") const -> nestdaq::telemetry::Gauge;

    auto StartSpan(std::string_view name,
                   std::initializer_list<Attribute> attributes) -> TelemetrySpan;

private:
    TelemetryLibrary* fLibrary{nullptr};
};

auto SetActiveTelemetryLibrary(TelemetryLibrary* library) noexcept -> void;
auto GetTelemetry() noexcept -> Telemetry;

} // namespace nestdaq::telemetry
