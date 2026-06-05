/** @file
 *  @brief Parses FairMQ channel throughput log records.
 */

#include "nestdaq/telemetry/FairMQThroughputLogParser.h"

#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <memory>
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
    const auto *last = std::to_address(token.end());
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

auto ParseChannel(std::string_view value, FairMQThroughputSample &sample) -> bool
{
    value = Trim(value);
    if (value.empty()) {
        return false;
    }

    sample.subChannelName = std::string{value};

    if (!value.ends_with(']')) {
        sample.channelName = std::string{value};
        return true;
    }

    const auto openBracket = value.rfind('[');
    if (openBracket == std::string_view::npos || openBracket == 0 || openBracket + 1 >= value.size() - 1) {
        return false;
    }

    const auto channelName = Trim(value.substr(0, openBracket));
    if (channelName.empty()) {
        return false;
    }

    const auto indexToken = value.substr(openBracket + 1, value.size() - openBracket - 2);
    uint64_t index = 0;
    const auto *first = indexToken.data();
    const auto *last = std::to_address(indexToken.end());
    const auto result = std::from_chars(first, last, index);
    if (result.ec != std::errc{} || result.ptr != last) {
        return false;
    }

    sample.channelName = std::string{channelName};
    sample.subChannelIndex = index;
    return true;
}

} // namespace

auto ParseFairMQThroughputLog(std::string_view line) -> std::optional<FairMQThroughputSample>
{
    line = Trim(line);
    const auto channelDelimiter = line.find(": in:");
    if (channelDelimiter == std::string_view::npos) {
        return std::nullopt;
    }

    auto input = line.substr(channelDelimiter + 2);
    auto sample = FairMQThroughputSample{};
    if (!ParseChannel(line.substr(0, channelDelimiter), sample)) {
        return std::nullopt;
    }

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
