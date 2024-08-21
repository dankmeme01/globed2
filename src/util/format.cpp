#include "format.hpp"

#include <cmath>
#include <sstream>
#include <iomanip>

#include <managers/web.hpp>

namespace util::format {
    std::string formatDateTime(const time::system_time_point& tp, bool ms) {
        auto timet = time::sysclock::to_time_t(tp);
        auto nowms = chrono::duration_cast<chrono::milliseconds>(tp.time_since_epoch()) % 1000;

        std::tm time_info = *std::localtime(&timet);

        std::ostringstream oss;
        oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
        if (ms) {
            oss << '.' << std::setfill('0') << std::setw(3) << nowms.count();
        }

        return oss.str();
    }

    std::string dateTime(const time::system_time_point& tp, bool ms) {
        return formatDateTime(tp, ms);
    }

    std::string formatBytes(uint64_t bytes) {
        // i did not write this myself
        static const char* suffixes[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"};

        if (bytes == 0) {
            return "0B";
        }

        int exp = static_cast<int>(std::log2(bytes) / 10);
        double value = static_cast<double>(bytes) / std::pow(1024, exp);

        std::ostringstream oss;

        if (std::fmod(value, 1.0) == 0) {
            oss << static_cast<int>(value);
        } else {
            oss << std::fixed << std::setprecision(1) << value;
        }

        oss << suffixes[exp];

        return oss.str();
    }

    std::string bytes(uint64_t bytes) {
        return formatBytes(bytes);
    }

    std::string formatErrorMessage(std::string message) {
        if (message.starts_with("ERR: ")) {
            auto errstr = std::string_view(message).substr(message.find("ERR: "));
            auto errid = errstr.substr(0, errstr.find(';'));

            return globed::string(errid);
        }

        if (message.find("<html>") != std::string::npos) {
            message = "<HTML response, not showing>";
        } else if (message.size() > 164) {
            message = message.substr(0, 164) + "...";
        }

        return message;
    }

    std::string formatPlatformerTime(uint32_t ms) {
        auto dur = time::millis(ms);
        auto hours = time::as<time::hours>(dur).count() % 24;
        auto minutes = time::as<time::minutes>(dur).count() % 60;
        auto seconds = time::as<time::seconds>(dur).count() % 60;
        auto millis = ms % 1000;

        if (hours > 0) {
            return fmt::format("{}:{:02}:{:02}.{:03}", hours, minutes, seconds, millis);
        } else if (minutes > 0) {
            return fmt::format("{}:{:02}.{:03}", minutes, seconds, millis);
        } else {
            return fmt::format("{}.{:03}", seconds, millis);
        }
    }

    Result<cocos2d::ccColor3B> parseColor(const std::string_view hex) {
        std::string_view subview;

        size_t hashPos = hex.find('#');
        if (hashPos == std::string::npos) {
            subview = hex;
        } else {
            subview = hex.substr(hashPos + 1);
        }

        auto result = parse<unsigned int>(subview, 16);
        GLOBED_REQUIRE_SAFE(result.has_value(), "failed to parse the hex color")

        int rgb = result.value();

        return Ok(cocos2d::ccColor3B {
            .r = static_cast<uint8_t>((rgb >> 16) & 0xff),
            .g = static_cast<uint8_t>((rgb >> 8) & 0xff),
            .b = static_cast<uint8_t>(rgb & 0xff)
        });
    }

    std::string colorToHex(cocos2d::ccColor3B color, bool withHash) {
        return fmt::format("{}{:02X}{:02X}{:02X}", withHash ? "#" : "", color.r, color.g, color.b);
    }

    std::string colorToHex(cocos2d::ccColor4B color, bool withHash) {
        return fmt::format("{}{:02X}{:02X}{:02X}{:02X}", withHash ? "#" : "", color.r, color.g, color.b, color.a);
    }

    std::string rtrim(const std::string_view str, const std::string_view filter) {
        size_t start = 0;
        size_t end = str.length();

        while (end > start && filter.find(str[end - 1]) != std::string::npos) {
            --end;
        }

        return std::string(str.substr(0, end));
    }

    std::string ltrim(const std::string_view str, const std::string_view filter) {
        size_t start = 0;
        size_t end = str.length();

        while (start < end && filter.find(str[start]) != std::string::npos) {
            ++start;
        }

        return std::string(str.substr(start));
    }

    std::string trim(const std::string_view str, const std::string_view filter) {
        return ltrim(rtrim(str, filter), filter);
    }

    std::string toLowercase(const std::string_view str) {
        std::string result;
        for (char c : str) {
            result += std::tolower(c);
        }

        return result;
    }

    std::string toUppercase(const std::string_view str) {
        std::string result;
        for (char c : str) {
            result += std::toupper(c);
        }

        return result;
    }

    std::string urlEncode(const std::string_view str) {
        std::ostringstream ss;
        ss.fill('0');
        ss << std::hex;

        for (char c : str) {
            if (std::isalnum(c) || c == '-' || c== '_' || c == '.' || c == '~') {
                ss << c;
                continue;
            }

            ss << std::uppercase;
            ss << '%' << std::setw(2) << static_cast<int>((unsigned char)c);
            ss << std::nouppercase;
        }

        return ss.str();
    }

    std::vector<std::string_view> split(const std::string_view s, const std::string_view sep) {
        std::vector<std::string_view> out;
        size_t start = 0;
        size_t end;

        while ((end = s.find(sep, start)) != std::string_view::npos) {
            out.emplace_back(s.substr(start, end - start));
            start = end + sep.size();
        }

        out.emplace_back(s.substr(start));

        return out;
    }

    std::vector<std::string_view> splitlines(const std::string_view s) {
        std::vector<std::string_view> lines;
        std::size_t start = 0;
        while (start < s.size()) {
            std::size_t end = s.find_first_of("\r\n", start);

            if (end == std::string_view::npos) {
                lines.emplace_back(s.substr(start));
                break;
            }

            lines.emplace_back(s.substr(start, end - start));
            if (s[end] == '\r' && end + 1 < s.size() && s[end + 1] == '\n') {
                end += 1;
            }

            start = end + 1;
        }

        return lines;
    }

    std::tuple<std::string_view, std::string_view, std::string_view> partition(std::string_view s, std::string_view sep) {
        auto sepidx = s.find(sep);
        if (sepidx == std::string::npos) {
            return std::make_tuple(s, "", "");
        }

        return std::make_tuple(s.substr(0, sepidx), sep, s.substr(sepidx + sep.size()));
    }

    std::tuple<std::string_view, std::string_view, std::string_view> rpartition(std::string_view s, std::string_view sep) {
        auto sepidx = s.rfind(sep);
        if (sepidx == std::string::npos) {
            return std::make_tuple(s, "", "");
        }

        return std::make_tuple(s.substr(0, sepidx), sep, s.substr(sepidx + sep.size()));
    }

    std::tuple<std::string_view, char, std::string_view> partition(std::string_view s, char sep) {
        auto sepidx = s.find(sep);
        if (sepidx == std::string::npos) {
            return std::make_tuple(s, '\0', "");
        }

        return std::make_tuple(s.substr(0, sepidx), sep, s.substr(sepidx + 1));
    }

    std::tuple<std::string_view, char, std::string_view> rpartition(std::string_view s, char sep) {
        auto sepidx = s.rfind(sep);
        if (sepidx == std::string::npos) {
            return std::make_tuple(s, '\0', "");
        }

        return std::make_tuple(s.substr(0, sepidx), sep, s.substr(sepidx + 1));
    }

    std::string replace(std::string_view input, std::string_view searched, std::string_view replacement) {
        std::string out;
        out.reserve(input.size());

        // input = 5 = abcde
        // searched = 2 = bc
        // i = 0 through 3 s

        for (size_t i = 0; i < input.size();) {
            // if does not match, simply append
            if (input.substr(i, searched.size()) != searched) {
                out += input[i];
                i++;
            } else {
                // if matches, increment i by size of searched and append the replacement
                out += replacement;
                i += searched.size();
            }
        }

        return out;
    }
}