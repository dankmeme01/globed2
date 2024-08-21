#pragma once

#include <charconv>
#include <cocos2d.h>
#include <defs/minimal_geode.hpp>

#include <util/time.hpp>
#include <util/misc.hpp>

namespace util::format {
    // example: 2.123s, 69.123ms
    template <typename Rep, typename Period>
    std::string formatDuration(const time::duration<Rep, Period>& time) {
        auto seconds = time::asSeconds(time);
        auto millis = time::asMillis(time);
        auto micros = time::asMicros(time);

        if (seconds > 0) {
            return fmt::format("{}.{:03}s", seconds, millis % 1000);
        } else if (millis > 0) {
            return fmt::format("{}.{:03}ms", millis, micros % 1000);
        } else {
            return std::to_string(micros) + "μs";
        }
    }

    template <typename Rep, typename Period>
    std::string duration(const time::duration<Rep, Period>& time) {
        return formatDuration(time);
    }

    // example: 2023-11-16 19:43:50.200
    std::string formatDateTime(const time::system_time_point& tp, bool ms = true);

    std::string dateTime(const time::system_time_point& tp, bool ms = true);

    // example: 123.4KiB
    std::string formatBytes(uint64_t bytes);

    std::string bytes(uint64_t bytes);

    // format an HTTP error message into a nicer string
    std::string formatErrorMessage(std::string message);

    // format milliseconds into a string like 45.200, 1:20.300, 3:01:40.500
    std::string formatPlatformerTime(uint32_t ms);

    // parse a string to an integer
    template <typename T>
    inline std::optional<T> parse(const std::string_view src, int base = 10) {
        T output;

        auto result = std::from_chars(&*src.begin(), &*src.end(), output, base);
        if (result.ec != std::errc()) {
            return std::nullopt;
        }

        return output;
    }

    Result<cocos2d::ccColor3B> parseColor(const std::string_view hex);

    std::string colorToHex(cocos2d::ccColor3B, bool withHash = false);
    std::string colorToHex(cocos2d::ccColor4B, bool withHash = false);

    std::string rtrim(const std::string_view str, const std::string_view filter = misc::STRING_WHITESPACE);
    std::string ltrim(const std::string_view str, const std::string_view filter = misc::STRING_WHITESPACE);
    std::string trim(const std::string_view str, const std::string_view filter = misc::STRING_WHITESPACE);

    std::string toLowercase(const std::string_view str);
    std::string toUppercase(const std::string_view str);

    std::string urlEncode(const std::string_view str);

    std::vector<std::string_view> split(const std::string_view s, const std::string_view sep);
    std::vector<std::string_view> splitlines(const std::string_view s);

    // like python's partition
    std::tuple<std::string_view, std::string_view, std::string_view> partition(std::string_view s, std::string_view sep);
    std::tuple<std::string_view, std::string_view, std::string_view> rpartition(std::string_view s, std::string_view sep);

    std::tuple<std::string_view, char, std::string_view> partition(std::string_view s, char sep);
    std::tuple<std::string_view, char, std::string_view> rpartition(std::string_view s, char sep);

    std::string replace(std::string_view input, std::string_view searched, std::string_view replacement);
}