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
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iterator>
#include <optional>
#include <utility>

#include "core/gcode_parser.hpp"
#include "core/utility.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"

namespace gcode {

struct EnterBootloader {
    /**
     * EnterBootloader uses the command string "dfu" instead of a gcode to be
     * more like other modules. There are no arguments and in the happy path
     * there is no response (because we reboot into the bootloader).
     * */
    using ParseResult = std::optional<EnterBootloader>;
    static constexpr auto prefix = std::array{'d', 'f', 'u'};
    static constexpr const char* response = "dfu OK\n";

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(EnterBootloader()), working);
    }
};

struct GetSystemInfo {
    /**
     * GetSystemInfo keys off gcode M115 and returns hardware and
     * software versions and serial number
     * */
    using ParseResult = std::optional<GetSystemInfo>;
    static constexpr auto prefix = std::array{'M', '1', '1', '5'};
    static constexpr std::size_t SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(
        InputIt write_to_buf, InLimit write_to_limit,
        std::array<char, SERIAL_NUMBER_LENGTH> serial_number,
        const char* fw_version, const char* hw_version) -> InputIt {
        static constexpr const char* prefix = "M115 FW:";
        auto written =
            write_string_to_iterpair(write_to_buf, write_to_limit, prefix);
        if (written == write_to_limit) {
            return written;
        }
        written = write_string_to_iterpair(written, write_to_limit, fw_version);
        if (written == write_to_limit) {
            return written;
        }
        static constexpr const char* hw_prefix = " HW:";
        written = write_string_to_iterpair(written, write_to_limit, hw_prefix);
        if (written == write_to_limit) {
            return written;
        }
        written = write_string_to_iterpair(written, write_to_limit, hw_version);
        if (written == write_to_limit) {
            return written;
        }
        static constexpr const char* sn_prefix = " SerialNo:";
        written = write_string_to_iterpair(written, write_to_limit, sn_prefix);
        if (written == write_to_limit) {
            return written;
        }
        written = write_string_to_iterpair(written, write_to_limit,
                                           serial_number.begin());
        if (written == write_to_limit) {
            return written;
        }
        static constexpr const char* suffix = " OK\n";
        return write_string_to_iterpair(written, write_to_limit, suffix);
    }

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetSystemInfo()), working);
    }
};

struct SetSerialNumber {
    /*
    ** Set Serial Number uses a random gcode, M996, adjacent to the firmware
    *update gcode, 997
    ** Format: M996 <SN>
    ** Example: M996 HSM02071521A4 sets serial number to HSM02071521A4
    */
    using ParseResult = std::optional<SetSerialNumber>;
    static constexpr auto prefix = std::array{'M', '9', '9', '6', ' '};
    static constexpr const char* response = "M996 OK\n";
    static constexpr std::size_t SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SERIAL_NUMBER_LENGTH> serial_number = {};
    errors::ErrorCode with_error = errors::ErrorCode::NO_ERROR;

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        auto after = working;
        bool found = false;
        for (auto index = working; index != limit && (!found); index++) {
            if (std::isspace(*index) || (*index == '\0')) {
                after = index;
                found = true;
            }
        }
        if ((after != working) && (std::distance(working, after) <
                                   static_cast<int>(SERIAL_NUMBER_LENGTH))) {
            std::array<char, SERIAL_NUMBER_LENGTH> serial_number_res = {};
            std::copy(working, after, serial_number_res.begin());
            return std::make_pair(ParseResult(SetSerialNumber{
                                      .serial_number = serial_number_res}),
                                  after);
        }
        if (std::distance(working, after) >
            static_cast<int>(SERIAL_NUMBER_LENGTH)) {
            return std::make_pair(
                ParseResult(SetSerialNumber{
                    .with_error =
                        errors::ErrorCode::SYSTEM_SERIAL_NUMBER_INVALID}),
                input);
        }
        return std::make_pair(ParseResult(), input);
    }
};

struct SetPeltierDebug {
    /**
     * SetPeltierDebug uses M104.D, debug version of M104. Sets the
     * PWM as a percentage between 0 and 1, and sets the direction
     * as either HEAT or COOL
     *
     * M104.D <L,R,C,A> P[0.0,1.0] <H,C>
     *
     * The power will be maintained at the specified level until either
     * - An error occurs
     * - An M104 is sent
     * - Another M104.D is sent
     *
     */
    using ParseResult = std::optional<SetPeltierDebug>;
    static constexpr auto prefix =
        std::array{'M', '1', '0', '4', '.', 'D', ' '};
    static constexpr const char* response = "M104.D OK\n";

    double power;
    PeltierDirection direction;
    PeltierSelection peltier_selection;

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        // Get the next non-whitespace character for peltier selection
        working = gobble_whitespace(working, limit);
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }
        auto selection_char = *working;
        PeltierSelection selection = ALL;
        std::advance(working, 1);
        switch (selection_char) {
            case 'L':
                selection = PeltierSelection::LEFT;
                break;
            case 'R':
                selection = PeltierSelection::RIGHT;
                break;
            case 'C':
                selection = PeltierSelection::CENTER;
                break;
            case 'A':
                selection = PeltierSelection::ALL;
                break;
            default:
                // Invalid peltier selection
                return std::make_pair(ParseResult(), input);
        }

        // Get the next non-whitespace character for temperature selection
        working = gobble_whitespace(working, limit);
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }
        // Skip prefix (P)
        std::advance(working, 1);
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }

        auto power = parse_value<float>(working, limit);
        if (!power.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto power_val = power.first.value();
        if ((power_val < 0) || (power_val > 1.0)) {
            return std::make_pair(ParseResult(), input);
        }
        working = power.second;

        // Get the next non-whitespace character for peltier selection
        working = gobble_whitespace(working, limit);
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }
        auto direction_char = *working;
        std::advance(working, 1);
        PeltierDirection dir = PELTIER_HEATING;
        switch (direction_char) {
            case 'H':
                dir = PELTIER_HEATING;
                break;
            case 'C':
                dir = PELTIER_COOLING;
                break;
            default:
                // Invalid direction selection
                return std::make_pair(ParseResult(), input);
        }

        return std::make_pair(
            ParseResult(SetPeltierDebug{.power = power_val,
                                        .direction = dir,
                                        .peltier_selection = selection}),
            working);
    }
};

struct GetLidTemperatureDebug {
    /**
     * GetLidTemperatureDebug uses M141.D, debug version of M141
     *
     * - Lid thermistor temperature (LT)
     * - Lid thermistor last ADC reading (LA)
     */
    using ParseResult = std::optional<GetLidTemperatureDebug>;
    static constexpr auto prefix = std::array{'M', '1', '4', '1', '.', 'D'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, double lid_temp,
                                    uint16_t lid_adc) -> InputIt {
        auto res = snprintf(&*buf, (limit - buf), "M141.D LT:%0.2f LA:%d OK\n",
                            static_cast<float>(lid_temp), lid_adc);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetLidTemperatureDebug()), working);
    }
};

struct GetPlateTemperatureDebug {
    /**
     * GetPlateTemperatureDebug uses M105.D, debug version of M105
     *
     * - Heat sink temp (HST)
     * - Front right temp (FRT)
     * - Front left temp (FLT)
     * - Front center temp (FCT)
     * - Back right temp (BRT)
     * - Back left temp (BLT)
     * - Back center temp (BCT)
     * - Heat sink ADC (HSA)
     * - Front right adc (FRA)
     * - Front left adc (FLA)
     * - Front center adc (FCA)
     * - Back right adc (BRA)
     * - Back left adc (BLA)
     * - Back center adc (BCA)
     */
    using ParseResult = std::optional<GetPlateTemperatureDebug>;
    static constexpr auto prefix = std::array{'M', '1', '0', '5', '.', 'D'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(
        InputIt buf, InLimit limit, double heat_sink_temp,
        double front_right_temp, double front_left_temp,
        double front_center_temp, double back_right_temp, double back_left_temp,
        double back_center_temp, uint16_t heat_sink_adc,
        uint16_t front_right_adc, uint16_t front_left_adc,
        uint16_t front_center_adc, uint16_t back_right_adc,
        uint16_t back_left_adc, uint16_t back_center_adc) -> InputIt {
        auto res = snprintf(&*buf, (limit - buf),
                            "M105.D HST:%0.2f FRT:%0.2f FLT:%0.2f FCT:%0.2f "
                            "BRT:%0.2f BLT:%0.2f BCT:%0.2f HSA:%d FRA:%d "
                            "FLA:%d FCA:%d BRA:%d BLA:%d BCA:%d OK\n",
                            static_cast<float>(heat_sink_temp),
                            static_cast<float>(front_right_temp),
                            static_cast<float>(front_left_temp),
                            static_cast<float>(front_center_temp),
                            static_cast<float>(back_right_temp),
                            static_cast<float>(back_left_temp),
                            static_cast<float>(back_center_temp), heat_sink_adc,
                            front_right_adc, front_left_adc, front_center_adc,
                            back_right_adc, back_left_adc, back_center_adc);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetPlateTemperatureDebug()), working);
    }
};

}  // namespace gcode
