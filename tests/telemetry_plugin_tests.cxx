/**
 * @file telemetry_plugin_tests.cxx
 * @brief Catch2 tests for loading `libnestdaq_otel.so` through the runtime loader.
 */

#include <catch2/catch_test_macros.hpp>

#include <fairlogger/Logger.h>

#include <nestdaq/telemetry/Telemetry.h>

#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace {

struct CoutCapture {
    std::ostringstream output;
    std::streambuf* oldBuffer{std::cout.rdbuf(output.rdbuf())};

    CoutCapture() = default;
    CoutCapture(const CoutCapture&) = delete;
    CoutCapture& operator=(const CoutCapture&) = delete;
    CoutCapture(CoutCapture&&) = delete;
    CoutCapture& operator=(CoutCapture&&) = delete;

    ~CoutCapture()
    {
        std::cout.rdbuf(oldBuffer);
    }
};

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

auto MetricsConsoleConfig() -> nestdaq_otel_config
{
    auto options = nestdaq::telemetry::TelemetryOptions{};
    options.logProtocol.clear();
    options.metricProtocol = "console";
    options.traceProtocol.clear();
    options.serviceName = "nestdaq-test";
    options.serviceNamespace = "nestdaq";
    options.serviceInstanceId = "test-instance";
    options.timeoutMs = 50;
    options.metricExportIntervalMs = 100;
    return nestdaq::telemetry::MakeConfig(options);
}

auto TraceConsoleConfig() -> nestdaq_otel_config
{
    auto options = nestdaq::telemetry::TelemetryOptions{};
    options.logProtocol.clear();
    options.metricProtocol.clear();
    options.traceProtocol = "console";
    options.serviceName = "nestdaq-test";
    options.serviceNamespace = "nestdaq";
    options.serviceInstanceId = "test-instance";
    options.timeoutMs = 50;
    return nestdaq::telemetry::MakeConfig(options);
}

auto LogsAndMetricsConsoleConfig() -> nestdaq_otel_config
{
    auto options = nestdaq::telemetry::TelemetryOptions{};
    options.logProtocol = "console";
    options.metricProtocol = "console";
    options.traceProtocol.clear();
    options.serviceName = "nestdaq-test";
    options.serviceNamespace = "nestdaq";
    options.serviceInstanceId = "test-instance";
    options.timeoutMs = 50;
    options.metricExportIntervalMs = 100;
    return nestdaq::telemetry::MakeConfig(options);
}

auto ExtractJsonLog(std::string_view logs, std::string_view root) -> nlohmann::json
{
    const auto marker = std::string{"{\""} + std::string{root} + "\":";
    const auto begin = logs.find(marker);
    REQUIRE(begin != std::string_view::npos);
    const auto lineEnd = logs.find('\n', begin);
    const auto jsonText = logs.substr(begin, lineEnd == std::string_view::npos ? logs.size() - begin : lineEnd - begin);
    return nlohmann::json::parse(jsonText);
}

} // namespace

TEST_CASE("telemetry plugin loads unified nestdaq_otel library", "[telemetry][plugin]")
{
    auto library = nestdaq::telemetry::TelemetryLibrary{};

    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    CHECK(library.InitializeWith(DisabledConfig()));
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);
}

TEST_CASE("telemetry plugin rejects severity values outside FairLogger range", "[telemetry][plugin]")
{
    auto library = nestdaq::telemetry::TelemetryLibrary{};

    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(DisabledConfig()));

    CHECK(library.SetMinSeverity(static_cast<int32_t>(fair::Severity::fatal)));
    CHECK_FALSE(library.SetMinSeverity(-1));
    CHECK_FALSE(library.SetMinSeverity(static_cast<int32_t>(fair::Logger::fSeverityNames.size())));

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

TEST_CASE("FairLogger severity attributes preserve original severity", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));

    LOG(warn) << "severity attribute probe";

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("severity attribute probe") != std::string::npos);
    CHECK(logs.find("severity_num       : 13") != std::string::npos);
    CHECK(logs.find("severity_text      : WARN") != std::string::npos);
    CHECK(logs.find("fairlogger.severity.number: 10") != std::string::npos);
    CHECK(logs.find("fairlogger.severity.text: WARN") != std::string::npos);
    CHECK(logs.find("log.severity.text:") == std::string::npos);
}

TEST_CASE("FairLogger logs include NestDAQ instance id attributes", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));
    REQUIRE(library.SetNestdaqInstanceId("sampler-0"));

    LOG(warn) << "nestdaq instance id probe";

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("nestdaq instance id probe") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id: sampler-0") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.name: sampler") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.index: 0") != std::string::npos);
}

TEST_CASE("FairLogger logs omit derived NestDAQ instance fields for non-indexed ids", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));
    REQUIRE(library.SetNestdaqInstanceId("sampler-main"));

    LOG(warn) << "nestdaq non indexed instance id probe";

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("nestdaq non indexed instance id probe") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id: sampler-main") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.name:") == std::string::npos);
    CHECK(logs.find("nestdaq.instance.index:") == std::string::npos);
}

TEST_CASE("FairLogger NestDAQ instance id is cleared on shutdown", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));
    REQUIRE(library.SetNestdaqInstanceId("sink-1"));
    LOG(warn) << "before instance id clear";
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    auto libraryAfterShutdown = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(libraryAfterShutdown.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(libraryAfterShutdown.InitializeWith(LogOnlyConfig()));
    LOG(warn) << "after instance id clear";
    libraryAfterShutdown.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("before instance id clear") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id: sink-1") != std::string::npos);
    const auto after = logs.find("after instance id clear");
    REQUIRE(after != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id:", after) == std::string::npos);
}

TEST_CASE("FairMQ build metadata is logged instead of stored as resource attributes", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    const auto nestdaqJson = ExtractJsonLog(logs, "nestdaq");
    const auto fairmqJson = ExtractJsonLog(logs, "fairmq");

    CHECK(nestdaqJson["nestdaq"]["version"]["string"].is_string());
    CHECK(nestdaqJson["nestdaq"]["version"]["major"].is_number_unsigned());
    CHECK(nestdaqJson["nestdaq"]["version"]["minor"].is_number_unsigned());
    CHECK(nestdaqJson["nestdaq"]["version"]["patch"].is_number_unsigned());
    CHECK(nestdaqJson["nestdaq"]["version"]["prerelease"].is_string());
    CHECK(nestdaqJson["nestdaq"]["build"]["type"].is_string());
    CHECK(nestdaqJson["nestdaq"]["git"]["commit_count"].is_number_unsigned());
    CHECK(nestdaqJson["nestdaq"]["git"]["commit_hash"].is_string());
    CHECK(nestdaqJson["nestdaq"]["git"]["branch"].is_string());
    CHECK(nestdaqJson["nestdaq"]["git"]["remote_url"].is_string());
    CHECK(nestdaqJson["nestdaq"]["git"]["commit_date"].is_string());

    CHECK(fairmqJson["fairmq"]["version"]["string"].is_string());
    CHECK(fairmqJson["fairmq"]["version"]["major"].is_number_unsigned());
    CHECK(fairmqJson["fairmq"]["version"]["minor"].is_number_unsigned());
    CHECK(fairmqJson["fairmq"]["version"]["patch"].is_number_unsigned());
    CHECK(fairmqJson["fairmq"]["version"]["git"].is_string());
    CHECK(fairmqJson["fairmq"]["build"]["type"].is_string());
    CHECK(fairmqJson["fairmq"]["source"]["repo_url"].is_string());
    CHECK(fairmqJson["fairmq"]["license"].is_string());
    CHECK(fairmqJson["fairmq"]["copyright"].is_string());

    REQUIRE(logs.find("{\"nestdaq\":") != std::string::npos);
    REQUIRE(logs.find("{\"fairmq\":") != std::string::npos);
    CHECK(logs.find("{\"nestdaq\":") < logs.find("{\"fairmq\":"));
    CHECK(logs.find("NestDAQ version:") == std::string::npos);
    CHECK(logs.find("FairMQ git_version:") == std::string::npos);
    CHECK(logs.find("fairmq.git_version") == std::string::npos);
    CHECK(logs.find("fairmq.build_type") == std::string::npos);
    CHECK(logs.find("fairmq.repo_url") == std::string::npos);
    CHECK(logs.find("fairmq.license") == std::string::npos);
    CHECK(logs.find("fairmq.copyright") == std::string::npos);
}

TEST_CASE("metrics console initializes and exports resource attributes", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(MetricsConsoleConfig()));

    auto telemetry = nestdaq::telemetry::Telemetry{library};
    CHECK(telemetry.AddDoubleCounter("probe.counter", 42.0, "1", "probe counter"));
    nestdaq::telemetry::SetActiveTelemetryLibrary(&library);
    auto userTelemetry = nestdaq::telemetry::GetTelemetry();
    CHECK(userTelemetry.Counter("user.messages.total", "1", "user messages")
              .Add(3.0, {{"channel", "data"}, {"running", true}, {"partition", uint64_t{2}}}));
    CHECK(userTelemetry.Histogram("user.decode.duration", "ms", "user decode duration")
              .Record(4.5, {{"channel", "data"}, {"attempt", int64_t{1}}, {"ratio", 0.5}}));
    CHECK(userTelemetry.Gauge("user.queue.depth", "1", "user queue depth")
              .Record(1234.0, {{"channel", "data"}, {"slot", uint64_t{2}}}));
    CHECK(userTelemetry.Gauge("user.queue.depth", "1", "user queue depth")
              .Record(9876.5, {{"channel", "data"}, {"slot", uint64_t{2}}}));

    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    nestdaq::telemetry::SetActiveTelemetryLibrary(nullptr);
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("probe.counter") != std::string::npos);
    CHECK(output.find("service.name") != std::string::npos);
    CHECK(output.find("nestdaq-test") != std::string::npos);
    CHECK(output.find("service.namespace") != std::string::npos);
    CHECK(output.find("service.instance.id") != std::string::npos);
    CHECK(output.find("test-instance") != std::string::npos);
    CHECK(output.find("user.messages.total") != std::string::npos);
    CHECK(output.find("user.decode.duration") != std::string::npos);
    CHECK(output.find("user.queue.depth") != std::string::npos);
    CHECK(output.find("channel") != std::string::npos);
    CHECK(output.find("data") != std::string::npos);
    CHECK(output.find("running") != std::string::npos);
    CHECK(output.find("partition") != std::string::npos);
    CHECK(output.find("attempt") != std::string::npos);
    CHECK(output.find("ratio") != std::string::npos);
    CHECK(output.find("slot") != std::string::npos);
    CHECK(output.find("9876.5") != std::string::npos);
}

TEST_CASE("user telemetry facade is no-op before a backend is registered", "[telemetry][plugin]")
{
    nestdaq::telemetry::SetActiveTelemetryLibrary(nullptr);

    auto telemetry = nestdaq::telemetry::GetTelemetry();
    CHECK(telemetry.Counter("unregistered.counter", "1", "unregistered counter").Add(1.0));
    CHECK(telemetry.Histogram("unregistered.histogram", "ms", "unregistered histogram").Record(2.0));
    CHECK(telemetry.Gauge("unregistered.gauge", "1", "unregistered gauge").Record(3.0));

    auto span = telemetry.StartSpan("unregistered-span", {{"component", "test"}});
    CHECK_FALSE(span.SetAttribute({"payload.bytes", int64_t{128}}));
}

TEST_CASE("user telemetry facade exports RAII spans and attributes", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(TraceConsoleConfig()));
    nestdaq::telemetry::SetActiveTelemetryLibrary(&library);

    {
        auto span = nestdaq::telemetry::GetTelemetry().StartSpan("user-decode", {{"channel", "data"}});
        CHECK(span.SetAttribute({"payload.bytes", int64_t{128}}));
        auto moved = std::move(span);
        CHECK(moved.SetAttribute({"ok", true}));
    }

    nestdaq::telemetry::SetActiveTelemetryLibrary(nullptr);
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("user-decode") != std::string::npos);
    CHECK(output.find("channel") != std::string::npos);
    CHECK(output.find("data") != std::string::npos);
    CHECK(output.find("payload.bytes") != std::string::npos);
    CHECK(output.find("ok") != std::string::npos);
}

TEST_CASE("process metrics export without FairLogger logs or MetricsPlugin", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(MetricsConsoleConfig()));

    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("process.cpu.usage_percent") != std::string::npos);
    CHECK(output.find("process.memory.rss_mib") != std::string::npos);
    CHECK(output.find("fairmq.channel.messages_per_second") == std::string::npos);
    CHECK(output.find("data: in:") == std::string::npos);
}

TEST_CASE("FairMQ throughput metrics export parsed rate log samples", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogsAndMetricsConsoleConfig()));

    LOG(info) << "data: in: 123 (4.5 MB) out: 6.7 (8.9 MB)";

    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("fairmq.channel.messages_per_second") != std::string::npos);
    CHECK(output.find("fairmq.channel.megabytes_per_second") != std::string::npos);
    CHECK(output.find("fairmq.channel.name") != std::string::npos);
    CHECK(output.find("network.io.direction") != std::string::npos);
}

TEST_CASE("disabled metric and trace signals are no-op through loaded plugin", "[telemetry][plugin]")
{
    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(DisabledConfig()));

    auto telemetry = nestdaq::telemetry::Telemetry{library};
    CHECK(telemetry.AddDoubleCounter("disabled.counter", 1.0, "1", "disabled counter"));
    CHECK(telemetry.RecordDoubleHistogram("disabled.histogram", 2.0, "ms", "disabled histogram"));
    CHECK(telemetry.RecordDoubleGauge("disabled.gauge", 3.0, "1", "disabled gauge"));

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
