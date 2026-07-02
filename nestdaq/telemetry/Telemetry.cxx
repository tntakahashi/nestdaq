/** @file
 *  @brief Implements the OpenTelemetry-unlinked user telemetry facade.
 */

#include "nestdaq/telemetry/Telemetry.h"

#include "nestdaq/telemetry/FairLoggerTelemetryLoader.h"

#include <algorithm>
#include <atomic>

namespace nestdaq::telemetry {

Attribute::Attribute(std::string_view key, std::string_view value)
    : fKey{key}
    , fStringValue{value}
{
}

Attribute::Attribute(std::string_view key, const char* value)
    : Attribute{key, value == nullptr ? std::string_view{} : std::string_view{value}}
{
}

Attribute::Attribute(std::string_view key, bool value)
    : fKey{key}
    , fType{NESTDAQ_OTEL_ATTRIBUTE_BOOL}
    , fBoolValue{value ? 1U : 0U}
{
}

auto Attribute::ToOtelAttribute() const noexcept -> nestdaq_otel_attribute
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

auto MakeOtelAttributes(const Attribute* attributes, std::size_t attributeCount) -> std::vector<nestdaq_otel_attribute>
{
    auto values = std::vector<nestdaq_otel_attribute> {};
    values.reserve(attributeCount);
#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 9)
    for (std::size_t index = 0; index < attributeCount; ++index) {
        values.push_back(attributes[index].ToOtelAttribute());
    }
#else
    std::for_each_n(attributes, attributeCount, [&values](const Attribute& attribute) {
        values.push_back(attribute.ToOtelAttribute());
    });
#endif
    return values;
}

auto MakeOtelAttributes(std::initializer_list<Attribute> attributes) -> std::vector<nestdaq_otel_attribute>
{
    return MakeOtelAttributes(attributes.begin(), attributes.size());
}

#if __cplusplus >= 202002L
auto MakeOtelAttributes(std::span<const Attribute> attributes) -> std::vector<nestdaq_otel_attribute>
{
    return MakeOtelAttributes(attributes.data(), attributes.size());
}
#endif

TelemetrySpan::TelemetrySpan(TelemetryLibrary& telemetry, uint64_t handle) noexcept
    : fTelemetry {
    &telemetry
}
, fHandle{handle}
{
}

TelemetrySpan::TelemetrySpan(TelemetrySpan&& other) noexcept
    : fTelemetry {
    other.fTelemetry
}
, fHandle{other.fHandle}
{
    other.fTelemetry = nullptr;
    other.fHandle = 0;
}

auto TelemetrySpan::operator=(TelemetrySpan&& other) noexcept -> TelemetrySpan&
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

TelemetrySpan::~TelemetrySpan()
{
    End();
}

auto TelemetrySpan::End() noexcept -> void
{
    if (fTelemetry != nullptr && fHandle != 0) {
        fTelemetry->SpanEnd(fHandle);
        fHandle = 0;
    }
}

auto TelemetrySpan::SetAttribute(const nestdaq_otel_attribute& attribute) -> bool
{
    return fTelemetry != nullptr && fHandle != 0 && fTelemetry->SpanSetAttribute(fHandle, attribute);
}

auto TelemetrySpan::SetAttribute(const Attribute& attribute) -> bool
{
    const auto otelAttribute = attribute.ToOtelAttribute();
    return SetAttribute(otelAttribute);
}

Counter::Counter(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description)
    : fLibrary{library}
    , fName{name}
    , fUnit{unit}
    , fDescription{description}
{
}

auto Counter::Add(double value, std::initializer_list<Attribute> attributes) const -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    const auto attrs = MakeOtelAttributes(attributes);
    return fLibrary->MetricAddDoubleCounter(fName, value, fUnit, fDescription, attrs.data(), attrs.size());
}

Histogram::Histogram(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description)
    : fLibrary{library}
    , fName{name}
    , fUnit{unit}
    , fDescription{description}
{
}

auto Histogram::Record(double value, std::initializer_list<Attribute> attributes) const -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    const auto attrs = MakeOtelAttributes(attributes);
    return fLibrary->MetricRecordDoubleHistogram(fName, value, fUnit, fDescription, attrs.data(), attrs.size());
}

Gauge::Gauge(TelemetryLibrary* library, std::string_view name, std::string_view unit, std::string_view description)
    : fLibrary{library}
    , fName{name}
    , fUnit{unit}
    , fDescription{description}
{
}

auto Gauge::Record(double value, std::initializer_list<Attribute> attributes) const -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    const auto attrs = MakeOtelAttributes(attributes);
    return fLibrary->MetricRecordDoubleGauge(fName, value, fUnit, fDescription, attrs.data(), attrs.size());
}

Telemetry::Telemetry(TelemetryLibrary& library) noexcept
    : fLibrary {
    &library
}
{
}

Telemetry::Telemetry(TelemetryLibrary* library) noexcept
    : fLibrary {
    library
}
{
}

auto Telemetry::AddDoubleCounter(std::string_view name,
                                 double value,
                                 std::string_view unit,
                                 std::string_view description,
                                 const nestdaq_otel_attribute* attributes,
                                 std::size_t attributeCount) -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    return fLibrary->MetricAddDoubleCounter(name, value, unit, description, attributes, attributeCount);
}

auto Telemetry::RecordDoubleHistogram(std::string_view name,
                                      double value,
                                      std::string_view unit,
                                      std::string_view description,
                                      const nestdaq_otel_attribute* attributes,
                                      std::size_t attributeCount) -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    return fLibrary->MetricRecordDoubleHistogram(name, value, unit, description, attributes, attributeCount);
}

auto Telemetry::RecordDoubleGauge(std::string_view name,
                                  double value,
                                  std::string_view unit,
                                  std::string_view description,
                                  const nestdaq_otel_attribute* attributes,
                                  std::size_t attributeCount) -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    return fLibrary->MetricRecordDoubleGauge(name, value, unit, description, attributes, attributeCount);
}

auto Telemetry::StartSpan(std::string_view name,
                          const nestdaq_otel_attribute* attributes,
                          std::size_t attributeCount) -> TelemetrySpan
{
    if (fLibrary == nullptr) {
        return {};
    }
    return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attributes, attributeCount)};
}

auto Telemetry::Counter(std::string_view name,
                        std::string_view unit,
                        std::string_view description) const -> nestdaq::telemetry::Counter
{
    return {fLibrary, name, unit, description};
}

auto Telemetry::Histogram(std::string_view name,
                          std::string_view unit,
                          std::string_view description) const -> nestdaq::telemetry::Histogram
{
    return {fLibrary, name, unit, description};
}

auto Telemetry::Gauge(std::string_view name,
                      std::string_view unit,
                      std::string_view description) const -> nestdaq::telemetry::Gauge
{
    return {fLibrary, name, unit, description};
}

auto Telemetry::StartSpan(std::string_view name, std::initializer_list<Attribute> attributes) -> TelemetrySpan
{
    if (fLibrary == nullptr) {
        return {};
    }
    const auto attrs = MakeOtelAttributes(attributes);
    return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attrs.data(), attrs.size())};
}

namespace {
/**
 * @brief Process-wide backend pointer used by user-facing convenience APIs.
 *
 * Ownership remains with the caller that loaded the telemetry plugin. Atomic
 * access lets FairMQ callbacks and user code read the active backend without
 * taking locks.
 */
auto ActiveTelemetryLibrary() noexcept -> std::atomic<TelemetryLibrary*>&
{
    static auto value = std::atomic<TelemetryLibrary*> {nullptr};
    return value;
}
} // namespace

auto SetActiveTelemetryLibrary(TelemetryLibrary* library) noexcept -> void
{
    ActiveTelemetryLibrary().store(library, std::memory_order_release);
}

auto CreateActiveSpdlogSink() -> std::shared_ptr<spdlog::sinks::sink>
{
    auto* library = ActiveTelemetryLibrary().load(std::memory_order_acquire);
    if (library == nullptr) {
        return {};
    }
    return library->CreateSpdlogSink();
}

auto GetTelemetry() noexcept -> Telemetry
{
    return Telemetry{ActiveTelemetryLibrary().load(std::memory_order_acquire)};
}

} // namespace nestdaq::telemetry
