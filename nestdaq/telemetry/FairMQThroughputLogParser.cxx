/** @file
 *  @brief Parses FairMQ channel throughput log records.
 */

#include "nestdaq/telemetry/FairMQThroughputLogParser.h"

#include <charconv>
#include <cmath>
#include <cctype>
#include <system_error>

namespace nestdaq::telemetry {
namespace {

auto ConsumeLiteral(std::string_view &input, std::string_view literal) noexcept -> bool
{
    if (!input.starts_with(literal)) {
        return false;
    }
    input.remove_prefix(literal.size());
    return true;
}

auto ConsumeSpaces(std::string_view &input) noexcept -> void
{
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())) != 0) {
        input.remove_prefix(1);
    }
}

auto ParseDoubleToken(std::string_view &input, double &value) noexcept -> bool
{
    ConsumeSpaces(input);
    const auto tokenEnd = input.find_first_of(" )");
    if (tokenEnd == 0 || tokenEnd == std::string_view::npos) {
        return false;
    }

    const auto token = input.substr(0, tokenEnd);
    const auto *first = token.data();
    const auto *last = token.data() + token.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last || !std::isfinite(value) || value < 0.0) {
        return false;
    }
    input.remove_prefix(tokenEnd);
    return true;
}

auto Trim(std::string_view value) noexcept -> std::string_view
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

} // namespace

auto ParseFairMQThroughputLog(std::string_view line) -> std::optional<FairMQThroughputSample>
{
    line = Trim(line);
    const auto channelDelimiter = line.find(": in:");
    if (channelDelimiter == std::string_view::npos) {
        return std::nullopt;
    }

    const auto channelName = Trim(line.substr(0, channelDelimiter));
    if (channelName.empty()) {
        return std::nullopt;
    }

    auto input = line.substr(channelDelimiter + 2);
    auto sample = FairMQThroughputSample{.channelName = std::string{channelName}};

    if (!ConsumeLiteral(input, "in:") ||
        !ParseDoubleToken(input, sample.messagesPerSecondIn)) {
        return std::nullopt;
    }
    ConsumeSpaces(input);
    if (!ConsumeLiteral(input, "(") ||
        !ParseDoubleToken(input, sample.megabytesPerSecondIn)) {
        return std::nullopt;
    }
    ConsumeSpaces(input);
    if (!ConsumeLiteral(input, "MB)")) {
        return std::nullopt;
    }
    ConsumeSpaces(input);
    if (!ConsumeLiteral(input, "out:") ||
        !ParseDoubleToken(input, sample.messagesPerSecondOut)) {
        return std::nullopt;
    }
    ConsumeSpaces(input);
    if (!ConsumeLiteral(input, "(") ||
        !ParseDoubleToken(input, sample.megabytesPerSecondOut)) {
        return std::nullopt;
    }
    ConsumeSpaces(input);
    if (!ConsumeLiteral(input, "MB)") || !Trim(input).empty()) {
        return std::nullopt;
    }

    return sample;
}

} // namespace nestdaq::telemetry
