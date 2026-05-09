#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>

namespace nestdaq::telemetry {

/**
 * @brief Parsed FairMQ channel throughput rates from the standard Device log line.
 */
struct FairMQThroughputSample {
    std::string channelName;
    std::string subChannelName;
    std::optional<uint64_t> subChannelIndex;
    double messagesPerSecondIn = 0.0;
    double megabytesPerSecondIn = 0.0;
    double messagesPerSecondOut = 0.0;
    double megabytesPerSecondOut = 0.0;
};

/**
 * @brief Parse FairMQ Device throughput log lines.
 *
 * Expected input:
 * `<channel>: in: <msg/s> (<MB/s> MB) out: <msg/s> (<MB/s> MB)` or
 * `<channel>[<index>]: in: <msg/s> (<MB/s> MB) out: <msg/s> (<MB/s> MB)`.
 *
 * The channel field is trimmed because FairMQ pads it with `std::setw()`.
 */
auto ParseFairMQThroughputLog(std::string_view line) -> std::optional<FairMQThroughputSample>;

} // namespace nestdaq::telemetry
