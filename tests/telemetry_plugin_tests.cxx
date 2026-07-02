/**
 * @file telemetry_plugin_tests.cxx
 * @brief Catch2 tests for loading `libnestdaq_otel.so` through the runtime loader.
 */

#include <catch2/catch_test_macros.hpp>

#if NESTDAQ_HAVE_SPDLOG
#  include <spdlog/spdlog.h>
#endif

#include <fairmq/Version.h>
#include <fairlogger/Logger.h>

#include <nestdaq/telemetry/FairLoggerTelemetryLoader.h>
#include <nestdaq/telemetry/Telemetry.h>

#if NESTDAQ_HAVE_SPDLOG
#  include <nestdaq/telemetry/SpdlogOpenTelemetrySink.h>
#  include <nestdaq/telemetry/SpdlogLogger.h>
#endif

#include <array>
#include <chrono>
#include <cstdint>
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

auto BaseConfig() -> nestdaq_otel_config
{
    auto config = nestdaq_otel_config{};
    config.size = sizeof(config);
    config.logs = nestdaq::telemetry::MakeSignalConfig(
                      "", nestdaq::telemetry::kDefaultLogHttpEndpoint, nestdaq::telemetry::kDefaultGrpcEndpoint, "", 1U);
    config.metrics = nestdaq::telemetry::MakeSignalConfig(
                         "", nestdaq::telemetry::kDefaultMetricHttpEndpoint, nestdaq::telemetry::kDefaultGrpcEndpoint, "", 1U);
    config.traces = nestdaq::telemetry::MakeSignalConfig(
                        "", nestdaq::telemetry::kDefaultTraceHttpEndpoint, nestdaq::telemetry::kDefaultGrpcEndpoint, "", 1U);
    config.service_name = "nestdaq-test";
    config.service_namespace = "nestdaq";
    config.service_instance_id = "test-instance";
    config.host_name = "test-host";
    config.nestdaq_instance_id = "";
    config.nestdaq_instance_id_status = "unresolved";
    config.fairmq_id = "";
    config.fairmq_device = "";
    config.fairmq_session = "";
    config.fairmq_transport = "";
    config.fairmq_git_version = FAIRMQ_GIT_VERSION;
    config.fairmq_build_type = FAIRMQ_BUILD_TYPE;
    config.fairmq_repo_url = FAIRMQ_REPO_URL;
    config.fairmq_license = FAIRMQ_LICENSE;
    config.fairmq_copyright = FAIRMQ_COPYRIGHT;
    config.min_severity = static_cast<int32_t>(fair::Severity::info);
    config.timeout_ms = nestdaq::telemetry::kDefaultTimeoutMs;
    config.metric_export_interval_ms = nestdaq::telemetry::kDefaultMetricExportIntervalMs;
    return config;
}

auto DisabledConfig() -> nestdaq_otel_config
{
    return BaseConfig();
}

auto LogOnlyConfig() -> nestdaq_otel_config
{
    auto config = BaseConfig();
    config.logs.protocol = "console";
    return config;
}

auto MetricsConsoleConfig() -> nestdaq_otel_config
{
    auto config = BaseConfig();
    config.metrics.protocol = "console";
    config.service_namespace = "nestdaq";
    config.service_instance_id = "test-instance";
    config.nestdaq_instance_id = "sampler-0";
    config.nestdaq_instance_id_status = "resolved";
    config.timeout_ms = 50;
    config.metric_export_interval_ms = 100;
    return config;
}

auto TraceConsoleConfig() -> nestdaq_otel_config
{
    auto config = BaseConfig();
    config.traces.protocol = "console";
    config.service_namespace = "nestdaq";
    config.service_instance_id = "test-instance";
    config.nestdaq_instance_id = "sampler-0";
    config.nestdaq_instance_id_status = "resolved";
    config.timeout_ms = 50;
    return config;
}

auto LogsAndMetricsConsoleConfig() -> nestdaq_otel_config
{
    auto config = BaseConfig();
    config.logs.protocol = "console";
    config.metrics.protocol = "console";
    config.service_namespace = "nestdaq";
    config.service_instance_id = "test-instance";
    config.nestdaq_instance_id = "sampler-0";
    config.nestdaq_instance_id_status = "resolved";
    config.timeout_ms = 50;
    config.metric_export_interval_ms = 100;
    return config;
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

auto CountOccurrences(std::string_view haystack, std::string_view needle) -> std::size_t
{
    auto count = std::size_t{0};
    auto offset = std::size_t{0};
    while (true) {
        offset = haystack.find(needle, offset);
        if (offset == std::string_view::npos) {
            return count;
        }
        ++count;
        offset += needle.size();
    }
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

TEST_CASE("FairLogger severity records OTel fields and original severity attributes", "[telemetry][plugin]")
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
    CHECK(logs.find("host.name: test-host") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id: sampler-0") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.name: sampler") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.index: 0") != std::string::npos);
}

TEST_CASE("FairLogger logs use unresolved resource before NestDAQ instance id is known", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));

    LOG(warn) << "early unresolved nestdaq instance id probe";

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("early unresolved nestdaq instance id probe") != std::string::npos);
    CHECK(logs.find("host.name: test-host") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id.status: unresolved") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id:") == std::string::npos);
}

TEST_CASE("FairLogger logs use resolved resource after NestDAQ instance id reinitialization", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));

    LOG(warn) << "before resolved nestdaq instance id";

    auto resolvedConfig = LogOnlyConfig();
    resolvedConfig.nestdaq_instance_id = "sampler-0";
    resolvedConfig.nestdaq_instance_id_status = "resolved";
    REQUIRE(library.InitializeWith(resolvedConfig));
    REQUIRE(library.SetNestdaqInstanceId("sampler-0"));

    LOG(warn) << "after resolved nestdaq instance id";

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("before resolved nestdaq instance id") != std::string::npos);
    CHECK(logs.find("after resolved nestdaq instance id") != std::string::npos);
    CHECK(logs.find("host.name: test-host") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id.status: unresolved") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id.status: resolved") != std::string::npos);
    CHECK(logs.find("nestdaq.instance.id: sampler-0") != std::string::npos);
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

#if NESTDAQ_HAVE_SPDLOG
TEST_CASE("spdlog sink exports logs independently from FairLogger instrumentation", "[telemetry][plugin][spdlog]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));
    REQUIRE(library.SetMinSeverity(static_cast<int32_t>(fair::Severity::fatal)));

    auto logger = spdlog::logger{"otel-spdlog-test", {nestdaq::telemetry::CreateSpdlogOpenTelemetrySink()}};
    logger.set_level(spdlog::level::trace);
    logger.warn("spdlog warning probe");

    LOG(warn) << "fairlogger warning filtered by fatal threshold";

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("spdlog warning probe") != std::string::npos);
    CHECK(logs.find("fairlogger warning filtered by fatal threshold") == std::string::npos);
    CHECK(logs.find("severity_num       : 13") != std::string::npos);
    CHECK(logs.find("severity_text      : WARN") != std::string::npos);
    CHECK(logs.find("spdlog.logger.name: otel-spdlog-test") != std::string::npos);
    CHECK(logs.find("spdlog.level: warn") != std::string::npos);
}

TEST_CASE("spdlog logger helper exports through active telemetry plugin", "[telemetry][plugin][spdlog]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));
    nestdaq::telemetry::SetActiveTelemetryLibrary(&library);

    auto logger = nestdaq::telemetry::CreateSpdlogLogger("helper-spdlog-test");
    logger->set_level(spdlog::level::trace);
    logger->info("spdlog helper probe");

    nestdaq::telemetry::SetActiveTelemetryLibrary(nullptr);
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("spdlog helper probe") != std::string::npos);
    CHECK(logs.find("spdlog.logger.name: helper-spdlog-test") != std::string::npos);
    CHECK(logs.find("spdlog.level: info") != std::string::npos);
}

TEST_CASE("spdlog sink records source location attributes", "[telemetry][plugin][spdlog]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogOnlyConfig()));

    auto logger = spdlog::logger{"otel-spdlog-source-test", {nestdaq::telemetry::CreateSpdlogOpenTelemetrySink()}};
    logger.set_level(spdlog::level::trace);
    logger.log(spdlog::source_loc{"source-file.cxx", 123, "source_function"},
               spdlog::level::err,
               "spdlog source probe");

    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto logs = capture.output.str();
    CHECK(logs.find("spdlog source probe") != std::string::npos);
    CHECK(logs.find("severity_num       : 17") != std::string::npos);
    CHECK(logs.find("severity_text      : ERROR") != std::string::npos);
    CHECK(logs.find("code.file.path: source-file.cxx") != std::string::npos);
    CHECK(logs.find("code.line.number: 123") != std::string::npos);
    CHECK(logs.find("code.function.name: source_function") != std::string::npos);
    CHECK(logs.find("thread.id") != std::string::npos);
}
#endif

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
    CHECK(userTelemetry.AddCounter("user.inferred.counter", 1, "1", "inferred counter"));
    CHECK(userTelemetry.RecordHistogram("user.inferred.histogram", uint64_t{4096}, "By", "inferred histogram"));
    CHECK(userTelemetry.RecordGauge("user.inferred.gauge", 12.5F, "1", "inferred gauge"));
    CHECK(userTelemetry.Counter("user.messages.total", "1", "user messages")
    .Add(3, {{"channel", "data"}, {"running", true}, {"partition", uint64_t{2}}}));
    CHECK(userTelemetry.Histogram("user.decode.duration", "ms", "user decode duration")
    .Record(4.5F, {{"channel", "data"}, {"attempt", int64_t{1}}, {"ratio", 0.5}}));
    CHECK(userTelemetry.Gauge("user.queue.depth", "1", "user queue depth")
    .Record(uint64_t{1234}, {{"channel", "data"}, {"slot", uint64_t{2}}}));
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
    CHECK(output.find("host.name") != std::string::npos);
    CHECK(output.find("test-host") != std::string::npos);
    CHECK(output.find("nestdaq.instance.id") != std::string::npos);
    CHECK(output.find("sampler-0") != std::string::npos);
    CHECK(output.find("nestdaq.instance.id.status") != std::string::npos);
    CHECK(output.find("resolved") != std::string::npos);
    CHECK(output.find("user.messages.total") != std::string::npos);
    CHECK(output.find("user.decode.duration") != std::string::npos);
    CHECK(output.find("user.queue.depth") != std::string::npos);
    CHECK(output.find("user.inferred.counter") != std::string::npos);
    CHECK(output.find("user.inferred.histogram") != std::string::npos);
    CHECK(output.find("user.inferred.gauge") != std::string::npos);
    CHECK(output.find("channel") != std::string::npos);
    CHECK(output.find("data") != std::string::npos);
    CHECK(output.find("running") != std::string::npos);
    CHECK(output.find("partition") != std::string::npos);
    CHECK(output.find("attempt") != std::string::npos);
    CHECK(output.find("ratio") != std::string::npos);
    CHECK(output.find("slot") != std::string::npos);
    CHECK(output.find("9876.5") != std::string::npos);
}

TEST_CASE("user telemetry facade accepts low-level attribute arrays", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(MetricsConsoleConfig()));

    auto telemetry = nestdaq::telemetry::Telemetry{library};
    auto attributes = std::array{
        nestdaq_otel_attribute{
            .key = "channel",
            .type = NESTDAQ_OTEL_ATTRIBUTE_STRING,
            .string_value = "data",
            .int_value = 0,
            .uint_value = 0,
            .double_value = 0.0,
            .bool_value = 0,
        },
        nestdaq_otel_attribute{
            .key = "slot",
            .type = NESTDAQ_OTEL_ATTRIBUTE_UINT64,
            .string_value = "",
            .int_value = 0,
            .uint_value = 2,
            .double_value = 0.0,
            .bool_value = 0,
        },
    };
    CHECK(telemetry.AddCounter(
              "lowlevel.counter", 1, "1", "low-level counter", attributes.data(), attributes.size()));
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("lowlevel.counter") != std::string::npos);
    CHECK(output.find("channel") != std::string::npos);
    CHECK(output.find("data") != std::string::npos);
    CHECK(output.find("slot") != std::string::npos);
}

#if __cplusplus >= 202002L
TEST_CASE("user telemetry facade accepts C++20 span attributes", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(MetricsConsoleConfig()));

    auto telemetry = nestdaq::telemetry::Telemetry{library};
    auto attributes = std::array{
        nestdaq_otel_attribute{
            .key = "channel",
            .type = NESTDAQ_OTEL_ATTRIBUTE_STRING,
            .string_value = "data",
            .int_value = 0,
            .uint_value = 0,
            .double_value = 0.0,
            .bool_value = 0,
        },
        nestdaq_otel_attribute{
            .key = "slot",
            .type = NESTDAQ_OTEL_ATTRIBUTE_UINT64,
            .string_value = "",
            .int_value = 0,
            .uint_value = 2,
            .double_value = 0.0,
            .bool_value = 0,
        },
    };
    CHECK(telemetry.AddCounter(
              "span.counter", 1, "1", "span counter", std::span<const nestdaq_otel_attribute> {attributes}));
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("span.counter") != std::string::npos);
    CHECK(output.find("channel") != std::string::npos);
    CHECK(output.find("data") != std::string::npos);
    CHECK(output.find("slot") != std::string::npos);
}
#endif

TEST_CASE("user telemetry facade is no-op before a backend is registered", "[telemetry][plugin]")
{
    nestdaq::telemetry::SetActiveTelemetryLibrary(nullptr);

    auto telemetry = nestdaq::telemetry::GetTelemetry();
    CHECK(telemetry.AddCounter("unregistered.counter", 1));
    CHECK(telemetry.RecordHistogram("unregistered.histogram", 2));
    CHECK(telemetry.RecordGauge("unregistered.gauge", 3));
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
    CHECK(output.find("host.name") != std::string::npos);
    CHECK(output.find("test-host") != std::string::npos);
    CHECK(output.find("nestdaq.instance.id") != std::string::npos);
    CHECK(output.find("sampler-0") != std::string::npos);
    CHECK(output.find("nestdaq.instance.id.status") != std::string::npos);
    CHECK(output.find("resolved") != std::string::npos);
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
    CHECK(output.find("process.cpu.time") != std::string::npos);
    CHECK(output.find("process.cpu.utilization") != std::string::npos);
    CHECK(output.find("process.memory.usage") != std::string::npos);
    CHECK(output.find("cpu.mode: user") != std::string::npos);
    CHECK(output.find("cpu.mode: system") != std::string::npos);
    CHECK(output.find("unit\t\t: s") != std::string::npos);
    CHECK(output.find("unit\t\t: 1") != std::string::npos);
    CHECK(output.find("unit\t\t: By") != std::string::npos);
    CHECK(output.find("process.cpu.usage_percent") == std::string::npos);
    CHECK(output.find("process.memory.rss_mib") == std::string::npos);
    CHECK(output.find("fairmq.channel.messages_per_second") == std::string::npos);
    CHECK(output.find("data: in:") == std::string::npos);
}

TEST_CASE("user force flush exports no framework metrics when no framework samples are pending", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(MetricsConsoleConfig()));

    CHECK(library.ForceFlush(nestdaq::telemetry::kDefaultTimeoutMs));
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("process.cpu.time") == std::string::npos);
    CHECK(output.find("process.cpu.utilization") == std::string::npos);
    CHECK(output.find("process.memory.usage") == std::string::npos);
    CHECK(output.find("process.cpu.usage_percent") == std::string::npos);
    CHECK(output.find("process.memory.rss_mib") == std::string::npos);
    CHECK(output.find("fairmq.channel.messages_per_second") == std::string::npos);
    CHECK(output.find("fairmq.channel.megabytes_per_second") == std::string::npos);
    CHECK(output.find("fairmq.state.id") == std::string::npos);
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

TEST_CASE("FairMQ throughput metrics are not re-exported without a new log sample", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(LogsAndMetricsConsoleConfig()));

    LOG(info) << "data: in: 123 (4.5 MB) out: 6.7 (8.9 MB)";

    const auto afterLog = capture.output.str();
    REQUIRE(afterLog.find("fairmq.channel.messages_per_second") != std::string::npos);
    REQUIRE(afterLog.find("fairmq.channel.megabytes_per_second") != std::string::npos);
    const auto messagesCount = CountOccurrences(afterLog, "fairmq.channel.messages_per_second");
    const auto megabytesCount = CountOccurrences(afterLog, "fairmq.channel.megabytes_per_second");

    CHECK(library.ForceFlush(nestdaq::telemetry::kDefaultTimeoutMs));
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(CountOccurrences(output, "fairmq.channel.messages_per_second") == messagesCount);
    CHECK(CountOccurrences(output, "fairmq.channel.megabytes_per_second") == megabytesCount);
}

TEST_CASE("FairMQ state metrics export transitions once", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(MetricsConsoleConfig()));

    library.RecordFrameworkFairMQState(12, "RUNNING");

    const auto afterState = capture.output.str();
    REQUIRE(afterState.find("fairmq.state.id") != std::string::npos);
    REQUIRE(afterState.find("fairmq.state.name") != std::string::npos);
    REQUIRE(afterState.find("RUNNING") != std::string::npos);
    const auto stateMetricCount = CountOccurrences(afterState, "fairmq.state.id");

    CHECK(library.ForceFlush(nestdaq::telemetry::kDefaultTimeoutMs));
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(CountOccurrences(output, "fairmq.state.id") == stateMetricCount);
}

TEST_CASE("framework metrics flush does not export user metrics", "[telemetry][plugin]")
{
    auto capture = CoutCapture{};

    auto library = nestdaq::telemetry::TelemetryLibrary{};
    REQUIRE(library.Load(NESTDAQ_OTEL_LIBRARY_PATH));
    REQUIRE(library.InitializeWith(MetricsConsoleConfig()));

    auto telemetry = nestdaq::telemetry::Telemetry{library};
    CHECK(telemetry.AddDoubleCounter("user.framework_isolation.counter", 1.0, "1", "framework isolation"));

    library.RecordFrameworkFairMQState(11, "READY");

    const auto afterFrameworkFlush = capture.output.str();
    CHECK(afterFrameworkFlush.find("fairmq.state.id") != std::string::npos);
    CHECK(afterFrameworkFlush.find("user.framework_isolation.counter") == std::string::npos);

    CHECK(library.ForceFlush(nestdaq::telemetry::kDefaultTimeoutMs));
    library.ShutdownTelemetry(nestdaq::telemetry::kDefaultTimeoutMs);

    const auto output = capture.output.str();
    CHECK(output.find("user.framework_isolation.counter") != std::string::npos);
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
