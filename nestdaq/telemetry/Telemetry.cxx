#include "nestdaq/telemetry/Telemetry.h"

#include "nestdaq/telemetry/FairLoggerTelemetryLoader.h"

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

auto MakeOtelAttributes(std::span<const Attribute> attributes) -> std::vector<nestdaq_otel_attribute>
{
    auto values = std::vector<nestdaq_otel_attribute>{};
    values.reserve(attributes.size());
    for (const auto& attribute : attributes) {
        values.push_back(attribute.ToOtelAttribute());
    }
    return values;
}

TelemetrySpan::TelemetrySpan(TelemetryLibrary& telemetry, uint64_t handle) noexcept
    : fTelemetry{&telemetry}
    , fHandle{handle}
{
}

TelemetrySpan::TelemetrySpan(TelemetrySpan&& other) noexcept
    : fTelemetry{other.fTelemetry}
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
    const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
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
    const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
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
    const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
    return fLibrary->MetricRecordDoubleGauge(fName, value, fUnit, fDescription, attrs.data(), attrs.size());
}

Telemetry::Telemetry(TelemetryLibrary& library) noexcept
    : fLibrary{&library}
{
}

Telemetry::Telemetry(TelemetryLibrary* library) noexcept
    : fLibrary{library}
{
}

auto Telemetry::AddDoubleCounter(std::string_view name,
                                 double value,
                                 std::string_view unit,
                                 std::string_view description,
                                 std::span<const nestdaq_otel_attribute> attributes) -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    return fLibrary->MetricAddDoubleCounter(name, value, unit, description, attributes.data(), attributes.size());
}

auto Telemetry::RecordDoubleHistogram(std::string_view name,
                                      double value,
                                      std::string_view unit,
                                      std::string_view description,
                                      std::span<const nestdaq_otel_attribute> attributes) -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    return fLibrary->MetricRecordDoubleHistogram(name, value, unit, description, attributes.data(), attributes.size());
}

auto Telemetry::RecordDoubleGauge(std::string_view name,
                                  double value,
                                  std::string_view unit,
                                  std::string_view description,
                                  std::span<const nestdaq_otel_attribute> attributes) -> bool
{
    if (fLibrary == nullptr) {
        return true;
    }
    return fLibrary->MetricRecordDoubleGauge(name, value, unit, description, attributes.data(), attributes.size());
}

auto Telemetry::StartSpan(std::string_view name, std::span<const nestdaq_otel_attribute> attributes) -> TelemetrySpan
{
    if (fLibrary == nullptr) {
        return {};
    }
    return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attributes.data(), attributes.size())};
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
    const auto attrs = MakeOtelAttributes(std::span<const Attribute>{attributes.begin(), attributes.size()});
    return TelemetrySpan{*fLibrary, fLibrary->SpanStart(name, attrs.data(), attrs.size())};
}

namespace {
auto ActiveTelemetryLibrary() noexcept -> std::atomic<TelemetryLibrary*>&
{
    static auto value = std::atomic<TelemetryLibrary*>{nullptr};
    return value;
}
} // namespace

auto SetActiveTelemetryLibrary(TelemetryLibrary* library) noexcept -> void
{
    ActiveTelemetryLibrary().store(library, std::memory_order_release);
}

auto GetTelemetry() noexcept -> Telemetry
{
    return Telemetry{ActiveTelemetryLibrary().load(std::memory_order_acquire)};
}

} // namespace nestdaq::telemetry
