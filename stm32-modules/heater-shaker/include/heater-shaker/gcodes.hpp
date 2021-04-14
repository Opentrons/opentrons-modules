/*
** Definitions of valid gcodes understood by the heater/shaker; intended to work
*with
** the gcode parser in gcode_parser.hpp
*/

#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <utility>

#include "heater-shaker/gcode_parser.hpp"

namespace gcode {

struct SetRPM {
    /*
    ** Set RPM uses the spindle-speed code from standard gcode, M3 (CW)
    ** Format: M3 S<RPM>
    ** Example: M3 S500 sets target rpm to 500
    */
    using ParseResult = std::optional<SetRPM>;
    uint32_t rpm;

    static auto write_response_into(char* buf, size_t available) -> size_t {
        static constexpr const char* response = "M3 OK\n";
        return std::copy(
                   response,
                   // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                   response + std::min(strlen(response), available), buf) -
               buf;
    }

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt>&&
        std::sized_sentinel_for<Limit, InputIt> static auto
        parse(const InputIt& input, Limit limit)
            -> std::pair<ParseResult, InputIt> {
        // minimal m3 command
        constexpr auto prefix = std::array{'M', '3', ' ', 'S'};

        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        auto value_res = parse_value<uint32_t>(working, limit);

        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(SetRPM{.rpm = static_cast<uint32_t>(
                                                     value_res.first.value())}),
                              value_res.second);
    }
};

struct SetTemperature {
    /*
    ** SetTemperature uses a standard set-tool-temperature gcode, M104
    ** Format: M104 S<temp>
    ** Example: M104 S25 sets target temperature to 25C
    */
    using ParseResult = std::optional<SetTemperature>;
    float temperature;

    static auto write_response_into(char* buf, size_t available) -> size_t {
        static constexpr const char* response = "M104 OK\n";
        return std::copy(
                   response,
                   // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                   response + std::min(strlen(response), available), buf) -
               buf;
    }

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt>&&
        std::sized_sentinel_for<Limit, InputIt> static auto
        parse(const InputIt& input, Limit limit)
            -> std::pair<ParseResult, InputIt> {
        constexpr auto prefix = std::array{'M', '1', '0', '4', ' ', 'S'};

        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        auto value_res = parse_value<float>(working, limit);

        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        if (value_res.first.value() < 0) {
            return std::make_pair(ParseResult(), input);
        }

        return std::make_pair(
            ParseResult(SetTemperature{.temperature = value_res.first.value()}),
            value_res.second);
    }
};

struct GetTemperature {
    /*
    ** GetTemperature keys off a standard get-tool-temperature gcode, M105
    ** Format: M105
    ** Example: M105
    */
    using ParseResult = std::optional<GetTemperature>;

    static auto write_response_into(char* buf, size_t available,
                                    uint32_t current_temperature,
                                    uint32_t setpoint_temperature) -> size_t {
        static constexpr const char* prefix = "M105 C";
        auto* next = std::copy(
            prefix,
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            prefix + std::min(strlen(prefix), available), buf);

        auto tochars_result =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            std::to_chars(next, buf + available, current_temperature);
        if (tochars_result.ec != std::errc()) {
            return available;
        }
        next = tochars_result.ptr;

        static constexpr const char* setpoint_prefix = " T";
        next = std::copy(
            setpoint_prefix,
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            setpoint_prefix +
                std::min(
                    strlen(setpoint_prefix),
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                    available - (next - buf)),
            next);

        tochars_result =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            std::to_chars(next, buf + available, setpoint_temperature);
        if (tochars_result.ec != std::errc()) {
            return available;
        }
        next = tochars_result.ptr;

        static constexpr const char* suffix = " OK\n";
        next = std::copy(
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            suffix, suffix + std::min(strlen(suffix), available - (next - buf)),
            next);
        return next - buf;
    }
    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt>&&
        std::sized_sentinel_for<Limit, InputIt> static auto
        parse(const InputIt& input, Limit limit)
            -> std::pair<ParseResult, InputIt> {
        constexpr auto prefix = std::array{'M', '1', '0', '5'};

        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetTemperature()), working);
    }
};

struct GetRPM {
    /*
    ** GetRPM keys off a random gcode that sometimes does the right thing since
    *it's not
    ** like it's standardized or anything, M123
    ** Format: M123
    ** Example: M123
    */

    static auto write_response_into(char* buf, size_t available,
                                    uint32_t current_rpm, uint32_t setpoint_rpm)
        -> size_t {
        static constexpr const char* prefix = "M123 C";
        auto* next = std::copy(
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            prefix, prefix + std::min(strlen(prefix), available), buf);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto tochars_result = std::to_chars(next, buf + available, current_rpm);
        if (tochars_result.ec != std::errc()) {
            return available;
        }
        next = tochars_result.ptr;

        static constexpr const char* setpoint_prefix = " T";
        next = std::copy(
            setpoint_prefix,
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            setpoint_prefix +
                std::min(strlen(setpoint_prefix), available - (next - buf)),
            next);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        tochars_result = std::to_chars(next, buf + available, setpoint_rpm);
        if (tochars_result.ec != std::errc()) {
            return available;
        }
        next = tochars_result.ptr;

        static constexpr const char* suffix = " OK\n";
        next = std::copy(
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            suffix, suffix + std::min(strlen(suffix), available - (next - buf)),
            next);
        return next - buf;
    }
    using ParseResult = std::optional<GetRPM>;
    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt>&&
        std::sized_sentinel_for<Limit, InputIt> static auto
        parse(const InputIt& input, Limit limit)
            -> std::pair<ParseResult, InputIt> {
        constexpr auto prefix = std::array{'M', '1', '2', '3'};

        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetRPM()), working);
    }
};
}  // namespace gcode
