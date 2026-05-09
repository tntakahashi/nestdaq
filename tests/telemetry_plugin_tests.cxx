/**
 * @file telemetry_plugin_tests.cxx
 * @brief Catch2 tests for loading `libnestdaq_otel.so` through the runtime loader.
 */

#include <catch2/catch_test_macros.hpp>

#include <fairlogger/Logger.h>

#include <nestdaq/telemetry/Telemetry.h>

#include <string_view>

namespace {

auto DisabledConfig() -> nestdaq_otel_config
{
    auto options = nestdaq::telemetry::TelemetryOptions{};
    options.logProtocol.clear();
    options.metricProtocol.clear();
    options.traceProtocol.clear();
    options.serviceName = "nestdaq-test";
    return nestdaq::telemetry::MakeConfig(options);
}

auto LogOnlyConfig() -> nestdaq_otel_config
{
    auto options = nestdaq::telemetry::TelemetryOptions{};
    options.logProtocol = "console";
    options.metricProtocol.clear();
    options.traceProtocol.clear();
    options.serviceName = "nestdaq-test";
    return nestdaq::telemetry::MakeConfig(options);
}

} // namespace

TEST_CASE("telemetry plugin loads unified nestdaq_otel library", "[telemetry][plugin]")
{
    auto library = nestdaq::telemetry::TelemetryLibrary{};

    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    CHECK(library.InitializeWith(DisabledConfig()));
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);
}

TEST_CASE("FairMQ throughput logs are safe when metrics are disabled", "[telemetry][plugin]")
{
    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));

    LOG(info) << "data: in: 123 (4.5 MB) out: 6.7 (8.9 MB)";

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);
}

TEST_CASE("disabled metric and trace signals are no-op through loaded plugin", "[telemetry][plugin]")
{
    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(DisabledConfig()));

    auto telemetry = nestdaq::telemetry::Telemetry{library};
    CHECK(telemetry.AddDoubleCounter("disabled.counter", 1.0, "1", "disabled counter"));
    CHECK(telemetry.RecordDoubleHistogram("disabled.histogram", 2.0, "ms", "disabled histogram"));

    auto span = telemetry.StartSpan("disabled-span");
    const auto attribute = nestdaq_otel_attribute{
        .key = "component",
        .type = NESTDAQ_OTEL_ATTRIBUTE_STRING,
        .string_value = "test",
        .int_value = 0,
        .uint_value = 0,
        .double_value = 0.0,
        .bool_value = 0,
    };
    CHECK_FALSE(span.SetAttribute(attribute));

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);
}
