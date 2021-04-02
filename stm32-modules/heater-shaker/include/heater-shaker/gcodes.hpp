/*
** Definitions of valid gcodes understood by the heater/shaker; intended to work
*with
** the gcode parser in gcode_parser.hpp
*/

#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
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
