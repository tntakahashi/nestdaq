/**
 * @file fairmq_throughput_log_parser_tests.cxx
 * @brief Catch2 tests for FairMQ throughput log parsing.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <nestdaq/telemetry/FairMQThroughputLogParser.h>

TEST_CASE("FairMQ throughput parser accepts the Device rate log format", "[telemetry][fairmq]")
{
    const auto sample = nestdaq::telemetry::ParseFairMQThroughputLog(
                            "data: in: 1234.5 (6.75 MB) out: 8.25 (0.5 MB)");

    REQUIRE(sample.has_value());
    CHECK(sample->channelName == "data");
    CHECK(sample->subChannelName == "data");
    CHECK_FALSE(sample->subChannelIndex.has_value());
    CHECK(sample->messagesPerSecondIn == Catch::Approx{1234.5});
    CHECK(sample->megabytesPerSecondIn == Catch::Approx{6.75});
    CHECK(sample->messagesPerSecondOut == Catch::Approx{8.25});
    CHECK(sample->megabytesPerSecondOut == Catch::Approx{0.5});
}

TEST_CASE("FairMQ throughput parser trims padded channel names", "[telemetry][fairmq]")
{
    const auto sample = nestdaq::telemetry::ParseFairMQThroughputLog(
                            "       pull: in: 1 (2 MB) out: 3 (4 MB)");

    REQUIRE(sample.has_value());
    CHECK(sample->channelName == "pull");
    CHECK(sample->subChannelName == "pull");
    CHECK_FALSE(sample->subChannelIndex.has_value());
}

TEST_CASE("FairMQ throughput parser splits indexed subchannels", "[telemetry][fairmq]")
{
    const auto sample = nestdaq::telemetry::ParseFairMQThroughputLog(
                            "       data[12]: in: 1 (2 MB) out: 3 (4 MB)");

    REQUIRE(sample.has_value());
    CHECK(sample->channelName == "data");
    CHECK(sample->subChannelName == "data[12]");
    REQUIRE(sample->subChannelIndex.has_value());
    CHECK(*sample->subChannelIndex == 12);
}

TEST_CASE("FairMQ throughput parser accepts exponent notation", "[telemetry][fairmq]")
{
    const auto sample = nestdaq::telemetry::ParseFairMQThroughputLog(
                            "push: in: 1.5e+03 (2.5e-01 MB) out: 0 (0 MB)");

    REQUIRE(sample.has_value());
    CHECK(sample->messagesPerSecondIn == Catch::Approx{1500.0});
    CHECK(sample->megabytesPerSecondIn == Catch::Approx{0.25});
    CHECK(sample->messagesPerSecondOut == Catch::Approx{0.0});
    CHECK(sample->megabytesPerSecondOut == Catch::Approx{0.0});
}

TEST_CASE("FairMQ throughput parser rejects unrelated logs", "[telemetry][fairmq]")
{
    CHECK_FALSE(nestdaq::telemetry::ParseFairMQThroughputLog("fair::mq::Device running...").has_value());
    CHECK_FALSE(nestdaq::telemetry::ParseFairMQThroughputLog("data: in: text (1 MB) out: 2 (3 MB)").has_value());
    CHECK_FALSE(nestdaq::telemetry::ParseFairMQThroughputLog(": in: 1 (2 MB) out: 3 (4 MB)").has_value());
    CHECK_FALSE(nestdaq::telemetry::ParseFairMQThroughputLog("data[x]: in: 1 (2 MB) out: 3 (4 MB)").has_value());
    CHECK_FALSE(nestdaq::telemetry::ParseFairMQThroughputLog("data[]: in: 1 (2 MB) out: 3 (4 MB)").has_value());
}
