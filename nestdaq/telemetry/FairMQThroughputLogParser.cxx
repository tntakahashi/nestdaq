/** @file
 *  @brief Parses FairMQ channel throughput log records.
 */

#include "nestdaq/telemetry/FairMQThroughputLogParser.h"

#include "nestdaq/telemetry/Compat.h"

#include <cmath>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <memory>

namespace nestdaq::telemetry {
namespace {

auto startsWith(std::string_view input, std::string_view literal) noexcept -> bool
{
    return input.size() >= literal.size() && input.substr(0, literal.size()) == literal;
}

auto endsWith(std::string_view input, char suffix) noexcept -> bool
{
    return !input.empty() && input.back() == suffix;
}

auto consumeLiteral(std::string_view &input, std::string_view literal) noexcept -> bool
{
    if (!startsWith(input, literal)) {
        return false;
    }
    input.remove_prefix(literal.size());
    return true;
}

auto consumeSpaces(std::string_view &input) noexcept -> void
{
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())) != 0) {
        input.remove_prefix(1);
    }
}

auto parseDoubleToken(std::string_view &input, double &value) -> bool
{
    consumeSpaces(input);
    const auto token_end = input.find_first_of(" )");
    if (token_end == 0 || token_end == std::string_view::npos) {
        return false;
    }

    const auto token = input.substr(0, token_end);
    if (!compat::ParseDouble(token, value) || value < 0.0) {
        return false;
    }
    input.remove_prefix(token_end);
    return true;
}

auto trim(std::string_view value) noexcept -> std::string_view
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

auto parseChannel(std::string_view value, FairMQThroughputSample &sample) -> bool
{
    value = trim(value);
    if (value.empty()) {
        return false;
    }

    sample.subChannelName = std::string{value};

    if (!endsWith(value, ']')) {
        sample.channelName = std::string{value};
        return true;
    }

    const auto open_bracket = value.rfind('[');
    if (open_bracket == std::string_view::npos || open_bracket == 0 || open_bracket + 1 >= value.size() - 1) {
        return false;
    }

    const auto channel_name = trim(value.substr(0, open_bracket));
    if (channel_name.empty()) {
        return false;
    }

    const auto index_token = value.substr(open_bracket + 1, value.size() - open_bracket - 2);
    uint64_t index = 0;
    if (!compat::ParseInteger(index_token, index)) {
        return false;
    }

    sample.channelName = std::string{channel_name};
    sample.subChannelIndex = index;
    return true;
}

} // namespace

auto ParseFairMQThroughputLog(std::string_view line) -> std::optional<FairMQThroughputSample>
{
    line = trim(line);
    const auto channel_delimiter = line.find(": in:");
    if (channel_delimiter == std::string_view::npos) {
        return std::nullopt;
    }

    auto input = line.substr(channel_delimiter + 2);
    auto sample = FairMQThroughputSample{};
    if (!parseChannel(line.substr(0, channel_delimiter), sample)) {
        return std::nullopt;
    }

    if (!consumeLiteral(input, "in:") ||
            !parseDoubleToken(input, sample.messagesPerSecondIn)) {
        return std::nullopt;
    }
    consumeSpaces(input);
    if (!consumeLiteral(input, "(") ||
            !parseDoubleToken(input, sample.megabytesPerSecondIn)) {
        return std::nullopt;
    }
    consumeSpaces(input);
    if (!consumeLiteral(input, "MB)")) {
        return std::nullopt;
    }
    consumeSpaces(input);
    if (!consumeLiteral(input, "out:") ||
            !parseDoubleToken(input, sample.messagesPerSecondOut)) {
        return std::nullopt;
    }
    consumeSpaces(input);
    if (!consumeLiteral(input, "(") ||
            !parseDoubleToken(input, sample.megabytesPerSecondOut)) {
        return std::nullopt;
    }
    consumeSpaces(input);
    if (!consumeLiteral(input, "MB)") || !trim(input).empty()) {
        return std::nullopt;
    }

    return sample;
}

} // namespace nestdaq::telemetry
