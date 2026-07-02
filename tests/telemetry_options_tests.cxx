/**
 * @file telemetry_options_tests.cxx
 * @brief Catch2 tests for telemetry option parsing and C ABI config mapping.
 */

#include <catch2/catch_test_macros.hpp>

#include <boost/program_options.hpp>
#include <fairmq/ProgOptions.h>
#include <fairlogger/Logger.h>
#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>

#include <array>
#include <boost/uuid/string_generator.hpp>
#include <cstdlib>
#include <exception>
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
        "NESTDAQ_SPDLOG_CONSOLE_PATTERN",
        "NESTDAQ_SPDLOG_NATIVE_CONSOLE",
        "NESTDAQ_SPDLOG_ASYNC",
        "NESTDAQ_SPDLOG_ASYNC_QUEUE_SIZE",
        "NESTDAQ_SPDLOG_ASYNC_THREAD_COUNT",
        "NESTDAQ_SPDLOG_ASYNC_OVERFLOW_POLICY",
    };

    for (const auto* name : names) {
        unsetenv(name); // NOLINT(concurrency-mt-unsafe)
    }
}

auto Parse(std::vector<std::string> arguments) -> nestdaq::telemetry::TelemetryOptions
{
    auto argv = std::vector<char*> {};
    argv.reserve(arguments.size());
    for (auto& argument : arguments) {
        argv.emplace_back(argument.data());
    }
    return nestdaq::telemetry::ParseTelemetryOptions(static_cast<int>(argv.size()), argv.data(), "test-service");
}

auto IsUuidString(std::string_view value) -> bool
{
    try {
        static_cast<void>(boost::uuids::string_generator{}(std::string{value}));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

auto ReadWithBoostOptions(std::vector<std::string> arguments,
                          std::string_view defaultServiceName) -> nestdaq::telemetry::TelemetryOptions
{
    namespace bpo = boost::program_options;
    auto argv = std::vector<char*> {};
    argv.reserve(arguments.size());
    for (auto& argument : arguments) {
        argv.emplace_back(argument.data());
    }

    auto description = bpo::options_description{"test"};
    nestdaq::telemetry::AddTelemetryOptions(description, defaultServiceName);
    auto vm = bpo::variables_map{};
    bpo::store(bpo::command_line_parser(static_cast<int>(argv.size()), argv.data()).options(description).run(), vm);
    bpo::notify(vm);
    return nestdaq::telemetry::ReadTelemetryOptions(vm, defaultServiceName);
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
    CHECK(options.metricExportIntervalMs == 1000);
    CHECK(options.spdlogConsolePattern == nestdaq::telemetry::kDefaultSpdlogConsolePattern);
    CHECK(options.spdlogNativeConsole);
    CHECK_FALSE(options.spdlogAsync);
    CHECK(options.spdlogAsyncQueueSize == nestdaq::telemetry::kDefaultSpdlogAsyncQueueSize);
    CHECK(options.spdlogAsyncThreadCount == nestdaq::telemetry::kDefaultSpdlogAsyncThreadCount);
    CHECK(options.spdlogAsyncOverflowPolicy == nestdaq::telemetry::kDefaultSpdlogAsyncOverflowPolicy);

    const auto config = nestdaq::telemetry::MakeConfig(options);
    CHECK(config.metric_export_interval_ms == 1000);
}

TEST_CASE("spdlog async options follow command line and environment", "[telemetry]")
{
    ClearTelemetryEnvironment();

    setenv("NESTDAQ_SPDLOG_ASYNC", "true", 1); // NOLINT(concurrency-mt-unsafe)
    setenv("NESTDAQ_SPDLOG_ASYNC_QUEUE_SIZE", "1024", 1); // NOLINT(concurrency-mt-unsafe)
    setenv("NESTDAQ_SPDLOG_ASYNC_THREAD_COUNT", "2", 1); // NOLINT(concurrency-mt-unsafe)
    setenv("NESTDAQ_SPDLOG_ASYNC_OVERFLOW_POLICY", "overrun_oldest", 1); // NOLINT(concurrency-mt-unsafe)

    const auto envOptions = Parse({"test"});
    CHECK(envOptions.spdlogAsync);
    CHECK(envOptions.spdlogAsyncQueueSize == 1024);
    CHECK(envOptions.spdlogAsyncThreadCount == 2);
    CHECK(envOptions.spdlogAsyncOverflowPolicy == "overrun_oldest");

    const auto cliOptions = Parse({
        "test",
        "--spdlog-async=false",
        "--spdlog-async-queue-size",
        "2048",
        "--spdlog-async-thread-count=3",
        "--spdlog-async-overflow-policy=discard_new",
    });
    CHECK_FALSE(cliOptions.spdlogAsync);
    CHECK(cliOptions.spdlogAsyncQueueSize == 2048);
    CHECK(cliOptions.spdlogAsyncThreadCount == 3);
    CHECK(cliOptions.spdlogAsyncOverflowPolicy == "discard_new");

    ClearTelemetryEnvironment();
}

TEST_CASE("spdlog async options normalize invalid values", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({
        "test",
        "--spdlog-async-queue-size=0",
        "--spdlog-async-thread-count=0",
        "--spdlog-async-overflow-policy=drop_everything",
    });

    CHECK(options.spdlogAsyncQueueSize == nestdaq::telemetry::kDefaultSpdlogAsyncQueueSize);
    CHECK(options.spdlogAsyncThreadCount == nestdaq::telemetry::kDefaultSpdlogAsyncThreadCount);
    CHECK(options.spdlogAsyncOverflowPolicy == nestdaq::telemetry::kDefaultSpdlogAsyncOverflowPolicy);
}

TEST_CASE("spdlog native console option follows command line and environment", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto cliOptions = Parse({"test", "--spdlog-native-console=false"});
    CHECK_FALSE(cliOptions.spdlogNativeConsole);

    setenv("NESTDAQ_SPDLOG_NATIVE_CONSOLE", "off", 1); // NOLINT(concurrency-mt-unsafe)
    const auto envOptions = Parse({"test"});
    CHECK_FALSE(envOptions.spdlogNativeConsole);

    const auto overrideOptions = Parse({"test", "--spdlog-native-console", "true"});
    CHECK(overrideOptions.spdlogNativeConsole);

    ClearTelemetryEnvironment();
}

TEST_CASE("spdlog console pattern follows command line and environment", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto cliOptions = Parse({"test", "--spdlog-console-pattern=[%n] %v"});
    CHECK(cliOptions.spdlogConsolePattern == "[%n] %v");

    setenv("NESTDAQ_SPDLOG_CONSOLE_PATTERN", "%l:%v", 1); // NOLINT(concurrency-mt-unsafe)
    const auto envOptions = Parse({"test"});
    CHECK(envOptions.spdlogConsolePattern == "%l:%v");

    const auto overrideOptions = Parse({"test", "--spdlog-console-pattern", "%v"});
    CHECK(overrideOptions.spdlogConsolePattern == "%v");

    ClearTelemetryEnvironment();
}

TEST_CASE("spdlog console pattern facade stores process setting", "[telemetry]")
{
    nestdaq::telemetry::SetSpdlogConsolePattern("%v");
    CHECK(nestdaq::telemetry::GetSpdlogConsolePattern() == "%v");

    nestdaq::telemetry::SetSpdlogConsolePattern(nestdaq::telemetry::kDefaultSpdlogConsolePattern);
    CHECK(nestdaq::telemetry::GetSpdlogConsolePattern() == nestdaq::telemetry::kDefaultSpdlogConsolePattern);
}

TEST_CASE("spdlog native console facade stores process setting", "[telemetry]")
{
    nestdaq::telemetry::SetSpdlogNativeConsoleEnabled(false);
    CHECK_FALSE(nestdaq::telemetry::GetSpdlogNativeConsoleEnabled());

    nestdaq::telemetry::SetSpdlogNativeConsoleEnabled(true);
    CHECK(nestdaq::telemetry::GetSpdlogNativeConsoleEnabled());
}

TEST_CASE("spdlog async facade stores process setting", "[telemetry]")
{
    nestdaq::telemetry::SetSpdlogAsyncOptions({
        .enabled = true,
        .queueSize = 4096,
        .threadCount = 2,
        .overflowPolicy = "discard_new",
    });

    const auto enabledOptions = nestdaq::telemetry::GetSpdlogAsyncOptions();
    CHECK(enabledOptions.enabled);
    CHECK(enabledOptions.queueSize == 4096);
    CHECK(enabledOptions.threadCount == 2);
    CHECK(enabledOptions.overflowPolicy == "discard_new");

    nestdaq::telemetry::SetSpdlogAsyncOptions({});
    const auto defaultOptions = nestdaq::telemetry::GetSpdlogAsyncOptions();
    CHECK_FALSE(defaultOptions.enabled);
    CHECK(defaultOptions.queueSize == nestdaq::telemetry::kDefaultSpdlogAsyncQueueSize);
    CHECK(defaultOptions.threadCount == nestdaq::telemetry::kDefaultSpdlogAsyncThreadCount);
    CHECK(defaultOptions.overflowPolicy == nestdaq::telemetry::kDefaultSpdlogAsyncOverflowPolicy);
}

TEST_CASE("spdlog async options are read through Boost options", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = ReadWithBoostOptions({
        "test",
        "--spdlog-async=true",
        "--spdlog-async-queue-size=512",
        "--spdlog-async-thread-count=2",
        "--spdlog-async-overflow-policy=overrun_oldest",
    }, "boost-default");

    CHECK(options.spdlogAsync);
    CHECK(options.spdlogAsyncQueueSize == 512);
    CHECK(options.spdlogAsyncThreadCount == 2);
    CHECK(options.spdlogAsyncOverflowPolicy == "overrun_oldest");
}

TEST_CASE("spdlog async environment survives Boost option defaults", "[telemetry]")
{
    ClearTelemetryEnvironment();

    setenv("NESTDAQ_SPDLOG_ASYNC", "true", 1); // NOLINT(concurrency-mt-unsafe)
    setenv("NESTDAQ_SPDLOG_ASYNC_QUEUE_SIZE", "1024", 1); // NOLINT(concurrency-mt-unsafe)
    setenv("NESTDAQ_SPDLOG_ASYNC_THREAD_COUNT", "2", 1); // NOLINT(concurrency-mt-unsafe)
    setenv("NESTDAQ_SPDLOG_ASYNC_OVERFLOW_POLICY", "discard_new", 1); // NOLINT(concurrency-mt-unsafe)

    const auto options = ReadWithBoostOptions({"daq-webctl"}, "daq-webctl");

    CHECK(options.spdlogAsync);
    CHECK(options.spdlogAsyncQueueSize == 1024);
    CHECK(options.spdlogAsyncThreadCount == 2);
    CHECK(options.spdlogAsyncOverflowPolicy == "discard_new");

    ClearTelemetryEnvironment();
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

TEST_CASE("telemetry service name follows DAQ service option for devices", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto optionsWithEquals = Parse({"test", "--service-name=Sampler"});
    const auto configWithEquals = nestdaq::telemetry::MakeConfig(optionsWithEquals);

    CHECK(std::string_view{configWithEquals.service_name} == "sampler");

    const auto optionsWithSpace = Parse({"test", "--service-name", "Processor"});
    const auto configWithSpace = nestdaq::telemetry::MakeConfig(optionsWithSpace);

    CHECK(std::string_view{configWithSpace.service_name} == "processor");
}

TEST_CASE("explicit telemetry service name overrides DAQ service option", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto optionsAfter = Parse({"test", "--service-name=Sampler", "--otel-service-name=NullDevice"});
    const auto configAfter = nestdaq::telemetry::MakeConfig(optionsAfter);

    CHECK(std::string_view{configAfter.service_name} == "nulldevice");

    const auto optionsBefore = Parse({"test", "--otel-service-name=explicit", "--service-name=Sampler"});
    const auto configBefore = nestdaq::telemetry::MakeConfig(optionsBefore);

    CHECK(std::string_view{configBefore.service_name} == "explicit");
}

TEST_CASE("telemetry service name keeps daq-webctl default through Boost options", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = ReadWithBoostOptions({"daq-webctl"}, "daq-webctl");
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(std::string_view{config.service_name} == "daq-webctl");
}

TEST_CASE("telemetry service name falls back to executable basename for devices", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"/opt/nestdaq/bin/Sink"});
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(std::string_view{config.service_name} == "sink");
}

TEST_CASE("telemetry service namespace defaults to nestdaq", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"test"});
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(std::string_view{config.service_namespace} == "nestdaq");
}

TEST_CASE("explicit telemetry service namespace overrides default", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"test", "--otel-service-namespace=custom"});
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(std::string_view{config.service_namespace} == "custom");
}

TEST_CASE("telemetry service namespace defaults to nestdaq through Boost options", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = ReadWithBoostOptions({"daq-webctl"}, "daq-webctl");
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(std::string_view{config.service_namespace} == "nestdaq");
}

TEST_CASE("telemetry service instance id defaults to a generated uuid", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"test"});
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(options.generatedServiceInstanceId);
    REQUIRE_FALSE(options.serviceInstanceId.empty());
    CHECK(IsUuidString(options.serviceInstanceId));
    CHECK(std::string_view{config.service_instance_id} == options.serviceInstanceId);
}

TEST_CASE("telemetry NestDAQ instance id resource starts unresolved", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"test"});
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(std::string_view{config.nestdaq_instance_id}.empty());
    CHECK(std::string_view{config.nestdaq_instance_id_status} == "unresolved");
}

TEST_CASE("telemetry host name resource is detected by default", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({"test"});
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(std::string_view{config.host_name} == options.hostName);
    if (!nestdaq::telemetry::DetectHostName().empty()) {
        CHECK_FALSE(options.hostName.empty());
    }
}

TEST_CASE("telemetry service instance id follows plugin uuid option", "[telemetry]")
{
    ClearTelemetryEnvironment();

    constexpr auto uuid = std::string_view{"123e4567-e89b-12d3-a456-426614174000"};
    const auto optionsWithEquals = Parse({"test", "--uuid=123e4567-e89b-12d3-a456-426614174000"});
    const auto configWithEquals = nestdaq::telemetry::MakeConfig(optionsWithEquals);

    CHECK_FALSE(optionsWithEquals.generatedServiceInstanceId);
    CHECK(std::string_view{configWithEquals.service_instance_id} == uuid);

    const auto optionsWithSpace = Parse({"test", "--uuid", "123e4567-e89b-12d3-a456-426614174000"});
    const auto configWithSpace = nestdaq::telemetry::MakeConfig(optionsWithSpace);

    CHECK_FALSE(optionsWithSpace.generatedServiceInstanceId);
    CHECK(std::string_view{configWithSpace.service_instance_id} == uuid);
}

TEST_CASE("explicit telemetry service instance id overrides plugin uuid option", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = Parse({
        "test",
        "--uuid=123e4567-e89b-12d3-a456-426614174000",
        "--otel-service-instance-id=explicit-instance",
    });
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK_FALSE(options.generatedServiceInstanceId);
    CHECK(std::string_view{config.service_instance_id} == "explicit-instance");
}

TEST_CASE("telemetry service instance id is generated through Boost options", "[telemetry]")
{
    ClearTelemetryEnvironment();

    const auto options = ReadWithBoostOptions({"daq-webctl"}, "daq-webctl");
    const auto config = nestdaq::telemetry::MakeConfig(options);

    CHECK(options.generatedServiceInstanceId);
    REQUIRE_FALSE(options.serviceInstanceId.empty());
    CHECK(IsUuidString(options.serviceInstanceId));
    CHECK(std::string_view{config.service_instance_id} == options.serviceInstanceId);
}

TEST_CASE("generated telemetry uuid populates missing FairMQ uuid property", "[telemetry]")
{
    ClearTelemetryEnvironment();

    auto options = Parse({"test"});
    auto config = fair::mq::ProgOptions{};

    nestdaq::telemetry::SetGeneratedUuidProperty(config, options);

    REQUIRE(config.Count("uuid") == 1);
    CHECK(config.GetProperty<std::string>("uuid") == options.serviceInstanceId);
}

TEST_CASE("generated telemetry uuid does not overwrite FairMQ uuid property", "[telemetry]")
{
    ClearTelemetryEnvironment();

    constexpr auto existingUuid = std::string_view{"123e4567-e89b-12d3-a456-426614174000"};
    auto options = Parse({"test"});
    auto config = fair::mq::ProgOptions{};
    config.SetProperty<std::string>("uuid", std::string{existingUuid});

    nestdaq::telemetry::SetGeneratedUuidProperty(config, options);

    CHECK(config.GetProperty<std::string>("uuid") == existingUuid);
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
