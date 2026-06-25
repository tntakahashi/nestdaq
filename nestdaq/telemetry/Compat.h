#pragma once

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#if !defined(__GNUC__) || defined(__clang__) || (__GNUC__ >= 11)
#include <charconv>
#include <system_error>
#define NESTDAQ_TELEMETRY_USE_STD_FROM_CHARS 1
#else
#define NESTDAQ_TELEMETRY_USE_STD_FROM_CHARS 0
#endif

namespace nestdaq::telemetry::compat {

inline auto ParseDouble(std::string_view token, double& value) -> bool
{
#if NESTDAQ_TELEMETRY_USE_STD_FROM_CHARS
    const auto* first = token.data();
    const auto* last = token.data() + token.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last && std::isfinite(value);
#else
    auto buffer = std::string{token};
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(buffer.c_str(), &end);
    if (errno == ERANGE || end == buffer.c_str() || end != buffer.c_str() + buffer.size() || !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
#endif
}

template <typename Integer>
inline auto ParseInteger(std::string_view token, Integer& value) -> bool
{
    static_assert(std::is_integral<Integer>::value, "ParseInteger requires an integral type");

#if NESTDAQ_TELEMETRY_USE_STD_FROM_CHARS
    const auto* first = token.data();
    const auto* last = token.data() + token.size();
    const auto result = std::from_chars(first, last, value);
    return result.ec == std::errc{} && result.ptr == last;
#else
    if (token.empty()) {
        return false;
    }

    auto buffer = std::string{token};
    char* end = nullptr;
    errno = 0;

    if constexpr (std::is_signed<Integer>::value) {
        const auto parsed = std::strtoll(buffer.c_str(), &end, 10);
        if (errno == ERANGE || end == buffer.c_str() || end != buffer.c_str() + buffer.size()) {
            return false;
        }
        if (parsed < static_cast<long long>(std::numeric_limits<Integer>::min()) ||
            parsed > static_cast<long long>(std::numeric_limits<Integer>::max())) {
            return false;
        }
        value = static_cast<Integer>(parsed);
        return true;
    } else {
        if (token.front() == '+' || token.front() == '-') {
            return false;
        }
        const auto parsed = std::strtoull(buffer.c_str(), &end, 10);
        if (errno == ERANGE || end == buffer.c_str() || end != buffer.c_str() + buffer.size()) {
            return false;
        }
        if (parsed > static_cast<unsigned long long>(std::numeric_limits<Integer>::max())) {
            return false;
        }
        value = static_cast<Integer>(parsed);
        return true;
    }
#endif
}

} // namespace nestdaq::telemetry::compat
