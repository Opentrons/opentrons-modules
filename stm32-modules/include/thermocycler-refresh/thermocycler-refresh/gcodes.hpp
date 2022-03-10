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
#include <iterator>
#include <optional>
#include <utility>

#include "core/gcode_parser.hpp"
#include "core/utility.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/motor_utils.hpp"
#include "thermocycler-refresh/tmc2130_registers.hpp"

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

// TODO this message needs to be expanded to include more info like on the
// arduino codebase. Will add after timeouts etc are included in control
// loops.
struct GetPlateTemp {
    /*
    ** GetPlateTemp keys off a standard get-tool-temperature gcode, M105
    ** Format: M105
    ** Example: M105
    **
    ** Returns the setpoint temperature T and the current temperature C
    **
    ** Returns T:none if the plate is off (setpoint = 0)
    */
    using ParseResult = std::optional<GetPlateTemp>;
    static constexpr auto prefix = std::array{'M', '1', '0', '5'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double current_temperature,
                                    double setpoint_temperature) -> InputIt {
        int res = 0;
        if (setpoint_temperature == 0.0F) {
            res = snprintf(&*buf, (limit - buf), "M105 T:none C:%0.2f OK\n",
                           static_cast<float>(current_temperature));
        } else {
            res = snprintf(&*buf, (limit - buf), "M105 T:%0.2f C:%0.2f OK\n",
                           static_cast<float>(setpoint_temperature),
                           static_cast<float>(current_temperature));
        }
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
        if (working != limit && !std::isspace(*working)) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetPlateTemp()), working);
    }
};

struct GetLidTemp {
    /*
    ** GetLidTemp uses gcode M141
    ** Format: M141
    ** Example: M141
    **
    ** Returns the setpoint temperature T and the current temperature C
    **
    ** Returns T:none if the plate is off (setpoint = 0)
    */
    using ParseResult = std::optional<GetLidTemp>;
    static constexpr auto prefix = std::array{'M', '1', '4', '1'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double current_temperature,
                                    double setpoint_temperature) -> InputIt {
        int res = 0;
        if (setpoint_temperature == 0.0F) {
            res = snprintf(&*buf, (limit - buf), "M141 T:none C:%0.2f OK\n",
                           static_cast<float>(current_temperature));
        } else {
            res = snprintf(&*buf, (limit - buf), "M141 T:%0.2f C:%0.2f OK\n",
                           static_cast<float>(setpoint_temperature),
                           static_cast<float>(current_temperature));
        }

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
        if (working != limit && !std::isspace(*working)) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetLidTemp()), working);
    }
};

struct SetFanManual {
    /**
     * SetFanManual uses M106. Sets the PWM of the fans as a percentage
     * between 0 and 1.
     *
     * M106 S[power]
     *
     * Power will be maintained at the specified level until:
     * - An error occurs
     * - Another M106 is set
     * - A Set Fan Auto command is sent
     * - The heatsink temperature exceeds the safety limit
     */
    using ParseResult = std::optional<SetFanManual>;
    static constexpr auto prefix = std::array{'M', '1', '0', '6', ' ', 'S'};
    static constexpr const char* response = "M106 OK\n";

    double power;

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        auto power_res = parse_value<float>(working, limit);

        if (!power_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto power_val = power_res.first.value();
        if ((power_val < 0.0) || (power_val > 1.0)) {
            return std::make_pair(ParseResult(), input);
        }
        working = power_res.second;
        return std::make_pair(ParseResult(SetFanManual{.power = power_val}),
                              working);
    }
};

struct SetFanAutomatic {
    /**
     * SetFanAutomatic uses M107. It has no parameters and just
     * activates automatic fan control.
     */
    using ParseResult = std::optional<SetFanAutomatic>;
    static constexpr auto prefix = std::array{'M', '1', '0', '7'};
    static constexpr const char* response = "M107 OK\n";

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
        return std::make_pair(ParseResult(SetFanAutomatic()), working);
    }
};

struct SetHeaterDebug {
    /**
     * SetHeaterDebug uses M140.D, debug version of M140.
     * Sets the PWM of the heater as a percentage between 0 and 1.
     *
     * M140.D S[power]
     *
     * Power will be maintained at the specified level until:
     * - An error occurs
     * - Another M140.D is set
     * - A SetLid command is sent
     */
    using ParseResult = std::optional<SetHeaterDebug>;
    static constexpr auto prefix =
        std::array{'M', '1', '4', '0', '.', 'D', ' ', 'S'};
    static constexpr const char* response = "M140.D OK\n";

    double power;

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        auto power_res = parse_value<float>(working, limit);

        if (!power_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto power_val = power_res.first.value();
        if ((power_val < 0.0) || (power_val > 1.0)) {
            return std::make_pair(ParseResult(), input);
        }
        working = power_res.second;
        return std::make_pair(ParseResult(SetHeaterDebug{.power = power_val}),
                              working);
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

/**
 * @brief Uses M103.D to get the current power output for all thermal elements.
 *
 * Format: M103.D\n
 * Return: M103.D L:<left peltier> C:<center> R:<right> H:<heater> F:<fans>
 *
 */
struct GetThermalPowerDebug {
    using ParseResult = std::optional<GetThermalPowerDebug>;
    static constexpr auto prefix = std::array{'M', '1', '0', '3', '.', 'D'};

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetThermalPowerDebug()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double left_power, double center_power,
                                    double right_power, double heater_power,
                                    double fan_power) -> InputIt {
        auto res = snprintf(
            &*buf, (limit - buf),
            "M103.D L:%0.2f C:%0.2f R:%0.2f H:%0.2f F:%0.2f OK\n",
            static_cast<float>(left_power), static_cast<float>(center_power),
            static_cast<float>(right_power), static_cast<float>(heater_power),
            static_cast<float>(fan_power));
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct ActuateSolenoid {
    /*
    ** Actuate solenoid is a debug command that lets you activate or deactivate
    ** the solenoid. It uses G28.D x where x is a bool and 1 engages and 0
    ** disengages the solenoid.
    */
    using ParseResult = std::optional<ActuateSolenoid>;
    static constexpr auto prefix = std::array{'G', '2', '8', '.', 'D', ' '};
    static constexpr const char* response = "G28.D OK\n";

    bool engage;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        auto engage_parse = parse_value<uint16_t>(working, limit);
        if (!engage_parse.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        bool tempEngage = static_cast<bool>(engage_parse.first.value());
        return std::make_pair(
            ParseResult(ActuateSolenoid{.engage = tempEngage}),
            engage_parse.second);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

/**
 * Actuate lid stepper is a debug command that lets you move the lid
 * stepper a desired angle. A positive value opens and negative value
 * closes the lid stepper a desired angle.
 *
 * Adding the character 'O' to the command marks an overdrive movement,
 * aka the limit switches will be ignored
 *
 * Format: M240.D <angle> [O]
 * Example: M240.D 20 opens lid stepper 20 degrees
 */
struct ActuateLidStepperDebug {
    using ParseResult = std::optional<ActuateLidStepperDebug>;
    static constexpr auto prefix =
        std::array{'M', '2', '4', '0', '.', 'D', ' '};
    static constexpr const char* response = "M240.D OK\n";

    static constexpr auto overdrive_flag = std::array{' ', 'O'};

    double angle;
    bool overdrive;

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        auto value_res = parse_value<float>(working, limit);
        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        working = prefix_matches(value_res.second, limit, overdrive_flag);
        // If the flag is present, working was incremented
        bool overdrive_set = (working != value_res.second);

        return std::make_pair(
            ParseResult(ActuateLidStepperDebug{.angle = value_res.first.value(),
                                               .overdrive = overdrive_set}),
            working);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct GetLidStatus {
    /**
     * GetLidStatus uses M119, from the Gen1 Thermocycler. Returns the
     * current status of the lid: in_between, closed, open, or unknown
     *
     * and the current status of the seal: in_between, engaged, retracted
     *
     * Format: M119
     * Returns: M119 Lid:open Seal:engaged OK\n
     */
    using ParseResult = std::optional<GetLidStatus>;
    static constexpr auto prefix = std::array{'M', '1', '1', '9'};

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetLidStatus()), working);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit,
                                    motor_util::LidStepper::Position lid,
                                    motor_util::SealStepper::Status seal)
        -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M119 Lid:%s Seal:%s OK\n",
                       motor_util::LidStepper::status_to_string(lid),
                       motor_util::SealStepper::status_to_string(seal));
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct ActuateSealStepperDebug {
    /*
    ** Actuate seal stepper is a debug command that lets you move the seal
    ** stepper a specific number of steps.
    ** Format: M241.D <steps>
    ** Example: M241.D 10000 opens lid stepper 10000 steps
    */
    using ParseResult = std::optional<ActuateSealStepperDebug>;
    static constexpr auto prefix =
        std::array{'M', '2', '4', '1', '.', 'D', ' '};
    static constexpr const char* response = "M241.D OK\n";

    long distance;

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        auto value_res = parse_value<long>(working, limit);
        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(ActuateSealStepperDebug{
                                  .distance = value_res.first.value()}),
                              value_res.second);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct GetSealDriveStatus {
    /**
     * GetSealDriverStatus uses M242.D. Returns the current status of the
     * DriverStatus register on the TMC2130
     *
     * Returns: M242.D SG:<stallguard flag> SG_Result:<stallguard result> OK\n
     */
    using ParseResult = std::optional<GetSealDriveStatus>;
    static constexpr auto prefix = std::array{'M', '2', '4', '2', '.', 'D'};

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetSealDriveStatus()), working);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit,
                                    tmc2130::DriveStatus status) -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M242.D SG:%u SG_Result:%u OK\n",
                       status.stallguard, status.sg_result);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct SetSealParameter {
    /**
     * @brief SetSealParameter uses M243.D. Lets users set parameters for the
     * seal stepper movement. Intended for internal testing use to find
     * optimal settings for StallGuard repeatability.
     *
     * Syntax: M243.D <parameter> <value>\n
     * Returns: M243.D OK\n
     *
     */
    using ParseResult = std::optional<SetSealParameter>;

    /** Enumeration of supported parameters.*/
    using SealParameter = motor_util::SealStepper::Parameter;

    static constexpr auto prefix =
        std::array{'M', '2', '4', '3', '.', 'D', ' '};
    static constexpr const char* response = "M243.D OK\n";

    /** Array of parameters to allow easy searching for legal parameters.*/
    static constexpr std::array<char, 6> _parameters = {
        static_cast<char>(SealParameter::Velocity),
        static_cast<char>(SealParameter::Acceleration),
        static_cast<char>(SealParameter::StallguardThreshold),
        static_cast<char>(SealParameter::StallguardMinVelocity),
        static_cast<char>(SealParameter::RunCurrent),
        static_cast<char>(SealParameter::HoldCurrent)};

    /** The parameter to set.*/
    SealParameter parameter;
    /** The value to set \c parameter to.*/
    int32_t value;

    template <typename Input>
    static auto inline is_legal_parameter(const Input parameter_char) -> bool {
        return std::find(_parameters.begin(), _parameters.end(),
                         parameter_char) != _parameters.end();
    }

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        // Next character should be one of the seal parameters
        auto parameter_char = *working;
        std::advance(working, 1);
        if (!is_legal_parameter(parameter_char)) {
            // Not a valid parameter
            return std::make_pair(ParseResult(), input);
        }
        working = gobble_whitespace(working, limit);
        if (working == limit) {
            // No value was defined
            return std::make_pair(ParseResult(), input);
        }

        auto value_res = parse_value<int32_t>(working, limit);
        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(
            ParseResult(SetSealParameter{
                .parameter = static_cast<SealParameter>(parameter_char),
                .value = value_res.first.value()}),
            value_res.second);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct SetLidTemperature {
    /**
     * SetLidTemperature uses M140. Only parameter is optional and it is
     * the temperature to heat to. If not defined, the temperature target
     * will be 105 degrees
     *
     * M140 S44\n
     */
    using ParseResult = std::optional<SetLidTemperature>;
    static constexpr auto prefix = std::array{'M', '1', '4', '0'};
    static constexpr auto prefix_with_temp =
        std::array{'M', '1', '4', '0', ' ', 'S'};
    static constexpr const char* response = "M140 OK\n";

    static constexpr double default_setpoint = 105.0F;

    double setpoint;

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
        auto working = prefix_matches(input, limit, prefix_with_temp);
        if (working == input) {
            // NO TEMP SETTING but it might just be a bare command
            working = prefix_matches(input, limit, prefix);
            if (working == input) {
                return std::make_pair(ParseResult(), input);
            }
            // Return a struct with default temperature
            return std::make_pair(
                ParseResult(SetLidTemperature{.setpoint = default_setpoint}),
                working);
        }
        // We are expecting a temperature setting
        auto temperature = parse_value<float>(working, limit);
        if (!temperature.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto temperature_val = temperature.first.value();
        return std::make_pair(
            ParseResult(SetLidTemperature{.setpoint = temperature_val}),
            temperature.second);
    }
};

struct DeactivateLidHeating {
    /**
     * DeactivateLidHeating uses M108. It has no parameters and just
     * deactivates the lid heater.
     */
    using ParseResult = std::optional<DeactivateLidHeating>;
    static constexpr auto prefix = std::array{'M', '1', '0', '8'};
    static constexpr const char* response = "M108 OK\n";

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
        return std::make_pair(ParseResult(DeactivateLidHeating()), working);
    }
};

struct SetPlateTemperature {
    /**
     * SetPlateTemperature uses M104. Parameters:
     * - S - setpoint temperature
     * - H - hold time (optional)
     *
     * M104 S44\n
     */
    using ParseResult = std::optional<SetPlateTemperature>;
    static constexpr auto prefix = std::array{'M', '1', '0', '4', ' ', 'S'};
    static constexpr auto hold_prefix = std::array{' ', 'H'};
    static constexpr const char* response = "M104 OK\n";

    // 0 seconds means infinite hold time
    constexpr static double infinite_hold = 0.0F;

    double setpoint;
    double hold_time;

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
        // We are expecting a temperature setting
        auto temperature = parse_value<float>(working, limit);
        if (!temperature.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto temperature_val = temperature.first.value();

        auto hold_val = infinite_hold;
        working = prefix_matches(temperature.second, limit, hold_prefix);
        if (working != temperature.second) {
            // This command specified a hold temperature
            auto hold = parse_value<float>(working, limit);
            if (!hold.first.has_value()) {
                return std::make_pair(ParseResult(), input);
            }
            hold_val = hold.first.value();
            working = hold.second;
        }

        return std::make_pair(
            ParseResult(SetPlateTemperature{.setpoint = temperature_val,
                                            .hold_time = hold_val}),
            working);
    }
};

struct DeactivatePlate {
    /**
     * DeactivatePlate uses M14. It has no parameters and just
     * deactivates the plate peltiers + fan.
     */
    using ParseResult = std::optional<DeactivatePlate>;
    static constexpr auto prefix = std::array{'M', '1', '4'};
    static constexpr const char* response = "M14 OK\n";

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
        return std::make_pair(ParseResult(DeactivatePlate()), working);
    }
};

struct SetPIDConstants {
    /**
     * SetPIDConstants uses M301. It has three parameters, along with
     * an optional preceding parameter (optional for backwards
     * compatability).
     *
     * M301 [S<selection>] P<proportional> I<integral> D<derivative>
     *
     * Selection may be:
     * - H = heater
     * - P = peltiers
     * - F = fans
     * [following options TBD]
     * - L = left peltier
     * - C = center peltier
     * - R = right peltier
     */
    using ParseResult = std::optional<SetPIDConstants>;
    static constexpr auto prefix = std::array{'M', '3', '0', '1'};
    static constexpr auto prefix_with_selection =
        std::array{'M', '3', '0', '1', ' ', 'S'};
    static constexpr auto prefix_p = std::array{' ', 'P'};
    static constexpr auto prefix_i = std::array{' ', 'I'};
    static constexpr auto prefix_d = std::array{' ', 'D'};
    static constexpr const char* response = "M301 OK\n";

    PidSelection selection;
    double const_p;
    double const_i;
    double const_d;

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
        // For backwards compatability, default selection is Peltiers
        PidSelection selection_val = PidSelection::PELTIERS;
        auto working = prefix_matches(input, limit, prefix_with_selection);
        if (working == input) {
            // User skipped the selection
            working = prefix_matches(input, limit, prefix);
            if (working == input) {
                return std::make_pair(ParseResult(), input);
            }
        } else {
            // User made a selection
            switch (*working) {
                case 'H':
                    selection_val = PidSelection::HEATER;
                    break;
                case 'P':
                    selection_val = PidSelection::PELTIERS;
                    break;
                case 'F':
                    selection_val = PidSelection::FANS;
                    break;
                default:
                    return std::make_pair(ParseResult(), input);
            }
            std::advance(working, 1);
            if (working == limit) {
                return std::make_pair(ParseResult(), input);
            }
        }

        auto old_working = working;
        working = prefix_matches(old_working, limit, prefix_p);
        if (working == old_working) {
            return std::make_pair(ParseResult(), input);
        }
        auto p = parse_value<float>(working, limit);
        if (!p.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        old_working = p.second;

        working = prefix_matches(old_working, limit, prefix_i);
        if (working == old_working) {
            return std::make_pair(ParseResult(), input);
        }
        auto i = parse_value<float>(working, limit);
        if (!p.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        old_working = i.second;

        working = prefix_matches(old_working, limit, prefix_d);
        if (working == old_working) {
            return std::make_pair(ParseResult(), input);
        }
        auto d = parse_value<float>(working, limit);
        if (!p.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        working = d.second;

        return std::make_pair(
            ParseResult(SetPIDConstants{.selection = selection_val,
                                        .const_p = p.first.value(),
                                        .const_i = i.first.value(),
                                        .const_d = d.first.value()}),
            working);
    }
};

/**
 * Uses M116, as defined on Gen 1 thermocyclers.
 *
 * Accepts two optional constants, B and C. These are
 * used in the calculation of the plate temperature for
 * each thermistor on the system with the following equation:
 *
 * > temp = (1+B)*(measured temp) + C
 *
 * Format: M116 B0.102 C-0.245\n
 *
 */
struct SetOffsetConstants {
    using ParseResult = std::optional<SetOffsetConstants>;
    static constexpr auto prefix = std::array{'M', '1', '1', '6'};
    static constexpr auto prefix_b = std::array{' ', 'B'};
    static constexpr auto prefix_c = std::array{' ', 'C'};
    static constexpr const char* response = "M116 OK\n";

    /**
     * Each constant is optional. In order to maintain simplicity
     * of this structure, rather than using std::optional we define
     * a field \c defined for each of the parameters.
     */
    struct OffsetConstant {
        bool defined;
        double value;
    };

    OffsetConstant const_b = {.defined = false, .value = 0.0F};
    OffsetConstant const_c = {.defined = false, .value = 0.0F};

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
        // Prefix with no variables is technically allowed
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(std::nullopt, input);
        }
        auto old_working = working;
        auto ret = SetOffsetConstants();
        working = prefix_matches(old_working, limit, prefix_b);
        if (working != old_working) {
            old_working = working;
            auto b = parse_value<float>(working, limit);
            if (!b.first.has_value()) {
                return std::make_pair(std::nullopt, input);
            }
            ret.const_b.defined = true;
            ret.const_b.value = b.first.value();
            working = b.second;
        }
        old_working = working;

        working = prefix_matches(old_working, limit, prefix_c);
        if (working != old_working) {
            old_working = working;
            auto c = parse_value<float>(working, limit);
            if (!c.first.has_value()) {
                return std::make_pair(std::nullopt, input);
            }
            ret.const_c.defined = true;
            ret.const_c.value = c.first.value();
            working = c.second;
        }
        return std::make_pair(ParseResult(ret), working);
    }
};

/**
 * Uses M117, as defined on Gen 1 thermocyclers.
 *
 * Returns the programmed offset constants on the device, B and C.
 *
 * Format: M117\n
 *
 * Returns: M117 B:[B value] C:[C value] OK\n
 *
 */
struct GetOffsetConstants {
    using ParseResult = std::optional<GetOffsetConstants>;
    static constexpr auto prefix = std::array{'M', '1', '1', '7'};

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetOffsetConstants()), working);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit, double b,
                                    double c) -> InputIt {
        auto res = snprintf(&*buf, (limit - buf), "M117 B:%0.2f C:%0.2f OK\n",
                            static_cast<float>(b), static_cast<float>(c));
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

/**
 * @brief Uses M126, same as gen 1 thermocycler. Opens the lid.
 *
 */
struct OpenLid {
    using ParseResult = std::optional<OpenLid>;
    static constexpr auto prefix = std::array{'M', '1', '2', '6'};
    static constexpr const char* response = "M126 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(OpenLid()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

/**
 * @brief Uses M127, same as gen 1 thermocycler. Closes the lid.
 *
 */
struct CloseLid {
    using ParseResult = std::optional<CloseLid>;
    static constexpr auto prefix = std::array{'M', '1', '2', '7'};
    static constexpr const char* response = "M127 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(CloseLid()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct GetBoardRevision {
    using ParseResult = std::optional<GetBoardRevision>;
    static constexpr auto prefix = std::array{'M', '9', '0', '0'};
    
    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetBoardRevision()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, int revision) -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M900 C:%i OK\n",
                       revision);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct GetLidSwitches {
    using ParseResult = std::optional<GetLidSwitches>;
    static constexpr auto prefix = std::array{'M', '9', '0', '1'};
    
    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetLidSwitches()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, int closed, int open) -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M901 C:%i O:%i OK\n",
                       closed, open);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }

};

}  // namespace gcode
