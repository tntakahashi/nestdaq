/**
 * @file telemetry_plugin_tests.cxx
 * @brief Catch2 tests for loading `libnestdaq_otel.so` through the runtime loader.
 */

#include <catch2/catch_test_macros.hpp>

#include <fairlogger/Logger.h>

#include <nestdaq/telemetry/Telemetry.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <string_view>

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
