/**
 * @file telemetry_options_tests.cxx
 * @brief Catch2 tests for telemetry option parsing and C ABI config mapping.
 */

#include <catch2/catch_test_macros.hpp>

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>

#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

auto ClearTelemetryEnvironment() -> void
{
    const auto names = std::array{
        "NESTDAQ_OTEL_LIBRARY",
        "NESTDAQ_OTEL_LOG_PROTOCOL",
        "NESTDAQ_OTEL_METRIC_PROTOCOL",
        "NESTDAQ_OTEL_TRACE_PROTOCOL",
        "NESTDAQ_OTEL_LOG_ENDPOINT_HTTP",
        "NESTDAQ_OTEL_LOG_ENDPOINT_GRPC",
        "NESTDAQ_OTEL_METRIC_ENDPOINT_HTTP",
        "NESTDAQ_OTEL_METRIC_ENDPOINT_GRPC",
        "NESTDAQ_OTEL_TRACE_ENDPOINT_HTTP",
        "NESTDAQ_OTEL_TRACE_ENDPOINT_GRPC",
        "NESTDAQ_OTEL_LOG_HEADERS",
        "NESTDAQ_OTEL_METRIC_HEADERS",
        "NESTDAQ_OTEL_TRACE_HEADERS",
        "NESTDAQ_OTEL_LOG_SEVERITY",
        "NESTDAQ_OTEL_LOG_REQUIRED",
    };

    for (const auto* name : names) {
        unsetenv(name); // NOLINT(concurrency-mt-unsafe)
    }
}

auto Parse(std::vector<std::string> arguments) -> nestdaq::telemetry::TelemetryOptions
{
    auto argv = std::vector<char*>{};
    argv.reserve(arguments.size());
    for (auto& argument : arguments) {
        argv.emplace_back(argument.data());
    }
    return nestdaq::telemetry::ParseTelemetryOptions(static_cast<int>(argv.size()), argv.data(), "test-service");
}

} // namespace

TEST_CASE("telemetry options keep unified otel library default", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"test"});

    CHECK(options.library == "libnestdaq_otel.so");
    CHECK(options.logProtocol == "console");
    CHECK(options.metricProtocol.empty());
    CHECK(options.traceProtocol.empty());
}

TEST_CASE("telemetry command line options populate multi-signal config", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({
        "test",
        "--otel-library=/tmp/libnestdaq_otel.so",
        "--otel-log-protocol=console,otlp-http",
        "--otel-metric-protocol=otlp-http",
        "--otel-trace-protocol=otlp-grpc",
        "--otel-log-endpoint-http=http://collector:4318/v1/logs",
        "--otel-log-endpoint-grpc=collector:4317",
        "--otel-metric-endpoint-http=http://collector:4318/v1/metrics",
        "--otel-metric-endpoint-grpc=collector:4317",
        "--otel-trace-endpoint-http=http://collector:4318/v1/traces",
        "--otel-trace-endpoint-grpc=collector:4317",
        "--otel-log-headers=log-key=log-value",
        "--otel-metric-headers=metric-key=metric-value",
        "--otel-trace-headers=trace-key=trace-value",
        "--otel-log-severity=warn",
        "--otel-timeout-ms=1234",
        "--otel-metric-export-interval-ms=5678",
        "--otel-log-http-json=false",
        "--otel-metric-http-json=true",
        "--otel-trace-http-json=false",
    });

    CHECK(options.library == "/tmp/libnestdaq_otel.so");
    CHECK(options.logProtocol == "console,otlp-http");
    CHECK(options.metricProtocol == "otlp-http");
    CHECK(options.traceProtocol == "otlp-grpc");

    const auto config = nestdaq::telemetry::MakeConfig(options);
    CHECK(config.size == sizeof(nestdaq_otel_config));
    CHECK(std::string_view{config.logs.protocol} == "console,otlp-http");
    CHECK(std::string_view{config.metrics.protocol} == "otlp-http");
    CHECK(std::string_view{config.traces.protocol} == "otlp-grpc");
    CHECK(std::string_view{config.logs.endpoint_http} == "http://collector:4318/v1/logs");
    CHECK(std::string_view{config.metrics.endpoint_http} == "http://collector:4318/v1/metrics");
    CHECK(std::string_view{config.traces.endpoint_grpc} == "collector:4317");
    CHECK(std::string_view{config.logs.headers} == "log-key=log-value");
    CHECK(std::string_view{config.metrics.headers} == "metric-key=metric-value");
    CHECK(std::string_view{config.traces.headers} == "trace-key=trace-value");
    CHECK(config.min_severity == static_cast<int32_t>(fair::Severity::warn));
    CHECK(config.timeout_ms == 1234);
    CHECK(config.metric_export_interval_ms == 5678);
    CHECK(config.logs.otlp_http_json == 0);
    CHECK(config.metrics.otlp_http_json == 1);
    CHECK(config.traces.otlp_http_json == 0);
}

TEST_CASE("telemetry log severity parsing follows FairLogger severity names", "[telemetry]")
{
    using nestdaq::telemetry::ParseFairLoggerSeverity;
    using nestdaq::telemetry::SeverityToFairLoggerValue;

    CHECK_FALSE(ParseFairLoggerSeverity("warn").usedFallback);
    CHECK(SeverityToFairLoggerValue("warn") == static_cast<int32_t>(fair::Severity::warn));
    CHECK(SeverityToFairLoggerValue("warning") == static_cast<int32_t>(fair::Severity::warn));
    CHECK(SeverityToFairLoggerValue("WARN") == static_cast<int32_t>(fair::Severity::warn));
    CHECK(SeverityToFairLoggerValue("fatal") == static_cast<int32_t>(fair::Severity::fatal));
    CHECK(SeverityToFairLoggerValue("NOLOG") == static_cast<int32_t>(fair::Severity::nolog));

    const auto unknownSeverity = ParseFairLoggerSeverity("unknown");
    CHECK(unknownSeverity.usedFallback);
    CHECK(unknownSeverity.value == static_cast<int32_t>(fair::Severity::info));
    CHECK(SeverityToFairLoggerValue("unknown") == static_cast<int32_t>(fair::Severity::info));
}

TEST_CASE("empty telemetry protocol disables the selected signal", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({
        "test",
        "--otel-log-protocol",
        "--otel-metric-protocol",
        "--otel-trace-protocol",
    });

    const auto config = nestdaq::telemetry::MakeConfig(options);
    CHECK(std::string_view{config.logs.protocol}.empty());
    CHECK(std::string_view{config.metrics.protocol}.empty());
    CHECK(std::string_view{config.traces.protocol}.empty());
}

TEST_CASE("removed otel-log-library option is ignored", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"test", "--otel-log-library=/tmp/old.so"});

    CHECK(options.library == "libnestdaq_otel.so");
}
