/*
** Definitions of valid gcodes understood by the heater/shaker; intended to work
*with
** the gcode parser in gcode_parser.hpp
*/

#pragma once

#include <printf.h>  // Non-malloc printf

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <utility>

#include "core/gcode_parser.hpp"
#include "core/utility.hpp"
#include "heater-shaker/errors.hpp"
#include "systemwide.h"

namespace gcode {

struct SetRPM {
    /*
    ** Set RPM uses the spindle-speed code from standard gcode, M3 (CW)
    ** Format: M3 S<RPM>
    ** Example: M3 S500 sets target rpm to 500
    */
    using ParseResult = std::optional<SetRPM>;
    static constexpr auto prefix = std::array{'M', '3', ' ', 'S'};
    static constexpr const char* response = "M3 OK\n";
    int16_t rpm;

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        // minimal m3 command

        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        auto value_res = parse_value<int16_t>(working, limit);

        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(SetRPM{.rpm = static_cast<int16_t>(
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
    static constexpr auto prefix = std::array{'M', '1', '0', '4', ' ', 'S'};
    static constexpr const char* response = "M104 OK\n";
    double temperature;

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InLimit, InputIt>
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
    static constexpr auto prefix = std::array{'M', '1', '0', '5'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double current_temperature,
                                    std::optional<double> setpoint_temperature)
        -> InputIt {
        int res = 0;
        if (setpoint_temperature) {
            res = snprintf(&*buf, (limit - buf), "M105 C:%0.2f T:%0.2f OK\n",
                           current_temperature, setpoint_temperature.value());
        } else {
            res = snprintf(&*buf, (limit - buf), "M105 C:%0.2f T:None OK\n",
                           current_temperature);
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
    static constexpr auto prefix = std::array{'M', '1', '2', '3'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InLimit, InputIt>
    static auto write_response_into(InputIt buf, const InLimit limit,
                                    int16_t current_rpm, int16_t setpoint_rpm)
        -> InputIt {
        static constexpr const char* prefix = "M123 C:";
        char* char_next = &*buf;
        char* const char_limit = &*limit;
        char_next = write_string_to_iterpair(char_next, char_limit, prefix);

        auto tochars_result = std::to_chars(char_next, char_limit, current_rpm);
        if (tochars_result.ec != std::errc()) {
            return buf + (tochars_result.ptr - &*buf);
        }
        char_next = tochars_result.ptr;

        static constexpr const char* setpoint_prefix = " T:";
        char_next =
            write_string_to_iterpair(char_next, char_limit, setpoint_prefix);

        tochars_result = std::to_chars(char_next, char_limit, setpoint_rpm);
        if (tochars_result.ec != std::errc()) {
            return buf + (tochars_result.ptr - &*buf);
        }
        char_next = tochars_result.ptr;

        static constexpr const char* suffix = " OK\n";
        char_next = write_string_to_iterpair(char_next, char_limit, suffix);
        return buf + (char_next - &*buf);
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
        return std::make_pair(ParseResult(GetRPM()), working);
    }
};

struct SetAcceleration {
    /*
    ** SetAcceleration uses M204 which is kind of the right thing. The
    *acceleration is in
    ** RPM/s.
    **
    ** Note: The spindle doesn't use linear acceleration all the time. This is
    *the ramp rate
    ** that will be followed for the majority of the time spent changing speeds.
    *It may be
    ** different when blending between ramp and constant speed control.
    ** Format: M204 Sxxxx
    ** Example: M204 S10000
    */

    using ParseResult = std::optional<SetAcceleration>;
    int32_t rpm_per_s;
    static constexpr auto prefix = std::array{'M', '2', '0', '4', ' ', 'S'};
    static constexpr const char* response = "M204 OK\n";

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }

    template <typename InputIt, typename Limit>
    requires std::contiguous_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        // minimal m3 command

        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }

        auto value_res = parse_value<int32_t>(working, limit);

        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(
            ParseResult(SetAcceleration{
                .rpm_per_s = static_cast<int32_t>(value_res.first.value())}),
            value_res.second);
    }
};

struct GetTemperatureDebug {
    /**
     * GetTemperatureDebug uses M105.D arbitrarily. It responds with
     *
     * - Pad A temperature (AT)
     * - Pad B temperature (BT)
     * - Board temperature (OT)
     * - Pad A last ADC reading (AD)
     * - Pad B last ADC reading (BD)
     * - Board last ADC reading (OD)
     * - power good (PG)
     * */
    using ParseResult = std::optional<GetTemperatureDebug>;
    static constexpr auto prefix = std::array{'M', '1', '0', '5', '.', 'D'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double pad_a_temp, double pad_b_temp,
                                    double board_temp, uint16_t pad_a_adc,
                                    uint16_t pad_b_adc, uint16_t board_adc,
                                    bool power_good) -> InputIt {
        auto res = snprintf(
            &*buf, (limit - buf),
            "M105.D AT:%0.2f BT:%0.2f OT:%0.2f AD:%d BD:%d OD:%d PG:%d OK\n",
            static_cast<float>(pad_a_temp), static_cast<float>(pad_b_temp),
            static_cast<float>(board_temp), pad_a_adc, pad_b_adc, board_adc,
            power_good ? 1 : 0);
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
        return std::make_pair(ParseResult(GetTemperatureDebug()), working);
    }
};

struct Home {
    /**
     * Home uses G28
     * */
    using ParseResult = std::optional<Home>;
    static constexpr auto prefix = std::array{'G', '2', '8'};
    static constexpr const char* response = "G28 OK\n";

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
        return std::make_pair(ParseResult(Home()), working);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct ActuateSolenoid {
    /*
    ** Actuate solenoid is a debug command that lets you activate or deactivate
    ** the solenoid. It uses G28.D Sxxxx where xxxx is an integer number of mA
    ** to use, e.g. G28.D S328 for 0.328A. If the value is 0, the solenoid will
    ** disengage.
    */
    using ParseResult = std::optional<ActuateSolenoid>;
    static constexpr auto prefix =
        std::array{'G', '2', '8', '.', 'D', ' ', 'S'};
    static constexpr const char* response = "G28.D OK\n";

    uint16_t current_ma;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        auto current_ma_parse = parse_value<uint16_t>(working, limit);
        if (!current_ma_parse.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        return std::make_pair(
            ParseResult(
                ActuateSolenoid{.current_ma = current_ma_parse.first.value()}),
            current_ma_parse.second);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    static auto write_response_into(InputIt buf, InputLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct SetPIDConstants {
    /**
     * SetPIDConstants uses M301 because smoothieware does. Parameters:
     * T[H|M]
     * Pxxx.xxx Ixxx.xxx Dxxx.xxx
     *
     * Example: M301 TH P1.02 I2.1 D1.0\r\n
     * */
    enum Target { HEATER, MOTOR };
    double kp;
    double ki;
    double kd;
    Target target;

    using ParseResult = std::optional<SetPIDConstants>;
    static constexpr auto prefix = std::array{'M', '3', '0', '1', ' ', 'T'};
    static constexpr const char* response = "M301 OK\n";

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
        Target target = HEATER;
        if (*working == 'M') {
            target = MOTOR;
        } else if (*working != 'H') {
            return std::make_pair(ParseResult(), input);
        }
        working++;
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }

        constexpr auto p_pref = std::array{' ', 'P'};
        auto before_pref = working;
        working = prefix_matches(working, limit, p_pref);
        if (working == before_pref) {
            return std::make_pair(ParseResult(), input);
        }

        auto kp_res = parse_value<float>(working, limit);

        if (!kp_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        constexpr auto i_pref = std::array{' ', 'I'};
        working = prefix_matches(kp_res.second, limit, i_pref);
        if (working == kp_res.second) {
            return std::make_pair(ParseResult(), input);
        }

        auto ki_res = parse_value<float>(working, limit);

        if (!ki_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        constexpr auto kd_pref = std::array{' ', 'D'};
        working = prefix_matches(ki_res.second, limit, kd_pref);
        if (working == ki_res.second) {
            return std::make_pair(ParseResult(), input);
        }
        auto kd_res = parse_value<float>(working, limit);
        if (!kd_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        return std::make_pair(
            ParseResult(SetPIDConstants{.kp = kp_res.first.value(),
                                        .ki = ki_res.first.value(),
                                        .kd = kd_res.first.value(),
                                        .target = target}),
            kd_res.second);
    }
};

struct SetHeaterPowerTest {
    /**
     * SetHeaterPowerTest is a testing command to directly command heater power
     * It uses M104.D to be like SetTemperature since it's the same kind of
     *thing.
     *
     * The argument should be between 1 and 0.
     * The power will be maintained at the specified level until either
     * - An error occurs
     * - An M104 is sent
     * - Another M104.D is sent
     *
     * A command of exactly 0 will turn off the power.
     *
     * While the system is in power test mode, M105 will return the power
     *setting as its target temperature, rather than a target temperature value.
     *The current temperature will still be the current temperature in C.
     *
     * Command: M104.D S0.124\n
     **/
    double power;
    using ParseResult = std::optional<SetHeaterPowerTest>;
    static constexpr auto prefix =
        std::array{'M', '1', '0', '4', '.', 'D', ' ', 'S'};
    static constexpr const char* response = "M104.D OK\n";

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

        auto power = parse_value<float>(working, limit);
        if (!power.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto power_val = power.first.value();
        if ((power_val < 0) || (power_val > 1.0)) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(
            ParseResult(SetHeaterPowerTest{.power = power_val}), power.second);
    }
};

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
    // If no SN is provided, this is the default rather than an empty string
    static constexpr const char* DEFAULT_SN = "EMPTYSN";

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
        auto fw_version_len = strlen(fw_version);
        static constexpr const char* sha_char = "-";
        auto sha_index = std::find(fw_version, fw_version + fw_version_len, sha_char);
        written = copy_min_range(written, write_to_limit, fw_version, sha_index);
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
        // If the serial number is unwritten, it will contain 0xFF which is
        // an illegal character that will confuse the host side. Replace the
        // first instance of it with a null terminator for safety.
        constexpr uint8_t invalid_ascii_mask = 0x80;
        auto serial_len = strnlen(serial_number.begin(), serial_number.size());
        auto invalid_char = std::find_if(
            serial_number.begin(), serial_number.end(), [](auto c) {
                return static_cast<uint8_t>(c) & invalid_ascii_mask;
            });
        if (invalid_char != serial_number.end()) {
            serial_len = std::min(serial_len,
                                  static_cast<size_t>(std::abs(std::distance(
                                      serial_number.begin(), invalid_char))));
        }
        if (serial_len > 0) {
            written =
                copy_min_range(written, write_to_limit, serial_number.begin(),
                               std::next(serial_number.begin(),
                                         static_cast<signed int>(serial_len)));
        } else {
            written =
                write_string_to_iterpair(written, write_to_limit, DEFAULT_SN);
        }
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
        for (int index = 0; (index != (limit - working + 1)) && (!found);
             index++) {
            if (std::isspace(*(working + index)) ||
                ((*(working + index)) == '\0')) {
                after = (working + index);
                found = true;
            }
        }
        if (((after - working) > 0) &&
            ((after - working) < static_cast<int>(SERIAL_NUMBER_LENGTH))) {
            std::array<char, SERIAL_NUMBER_LENGTH> serial_number_res = {};
            std::copy(working, (working + (after - working)),
                      serial_number_res.begin());
            return std::make_pair(ParseResult(SetSerialNumber{
                                      .serial_number = serial_number_res}),
                                  after);
        } else if (((after - working) > 0) &&
                   ((after - working) >=
                    static_cast<int>(SERIAL_NUMBER_LENGTH))) {
            return std::make_pair(
                ParseResult(SetSerialNumber{
                    .with_error =
                        errors::ErrorCode::SYSTEM_SERIAL_NUMBER_INVALID}),
                input);
        } else {
            return std::make_pair(ParseResult(), input);
        }
    }
};

struct SetLEDDebug {
    /*
    ** Set LED Debug uses a random gcode, M994.D
    ** Format: M994.D <which_LED_color>
    ** Example: M994.D 0 selects WHITE color and turns the white LEDs on
    */
    using ParseResult = std::optional<SetLEDDebug>;
    static constexpr auto prefix =
        std::array{'M', '9', '9', '4', '.', 'D', ' '};
    static constexpr const char* response = "M994.D OK\n";
    LED_COLOR color;

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

        auto value_res = parse_value<uint8_t>(working, limit);

        if (!value_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        LED_COLOR tempColor = static_cast<LED_COLOR>(value_res.first.value());
        return std::make_pair(ParseResult(SetLEDDebug{.color = tempColor}),
                              value_res.second);
    }
};

struct IdentifyModuleStartLED {
    /*
    ** IdentifyModuleStartLED uses a random gcode, M994, to start blinking the
    *white LED
    ** Format: M994
    ** Example: M994 starts blinking the white LED
    */
    using ParseResult = std::optional<IdentifyModuleStartLED>;
    static constexpr auto prefix = std::array{'M', '9', '9', '4'};
    static constexpr const char* response = "M994 OK\n";

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
        if (working != limit && !std::isspace(*working)) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(IdentifyModuleStartLED()), working);
    }
};

struct IdentifyModuleStopLED {
    /*
    ** IdentifyModuleStopLED uses a random gcode, M995, to stop blinking the
    *white LED
    ** Format: M995
    ** Example: M995 stops blinking the white LED
    */
    using ParseResult = std::optional<IdentifyModuleStopLED>;
    static constexpr auto prefix = std::array{'M', '9', '9', '5'};
    static constexpr const char* response = "M995 OK\n";

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
        if (working != limit && !std::isspace(*working)) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(IdentifyModuleStopLED()), working);
    }
};

struct DebugControlPlateLockMotor {
    /**
     * DebugControlPlateLockMotor is M240.D because why not.
     *
     * Arguments:
     *   - S(-)x.y float between 1 and -1 describing percentage of power to send
     *     to the motor (and direction). 0 or -0 turns off the motor entirely.
     *
     * Acknowledged immediately upon receipt
     * */
    using ParseResult = std::optional<DebugControlPlateLockMotor>;
    static constexpr auto prefix =
        std::array{'M', '2', '4', '0', '.', 'D', ' ', 'S'};
    static constexpr const char* response = "M240.D OK\n";

    float power;

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt write_to_buf,
                                    InLimit write_to_limit) {
        return write_string_to_iterpair(write_to_buf, write_to_limit, response);
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
        auto power_res = parse_value<float>(working, limit);
        if (!power_res.first.has_value() || power_res.second == working) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(DebugControlPlateLockMotor{
                                  .power = power_res.first.value()}),
                              power_res.second);
    }
};

struct OpenPlateLock {
    /**
     * OpenPlateLock is M242 based on existing convention
     *
     * Acknowledged immediately upon receipt
     * */
    using ParseResult = std::optional<OpenPlateLock>;
    static constexpr auto prefix = std::array{'M', '2', '4', '2'};
    static constexpr const char* response = "M242 OK\n";

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt write_to_buf,
                                    InLimit write_to_limit) {
        return write_string_to_iterpair(write_to_buf, write_to_limit, response);
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
        return std::make_pair(ParseResult(OpenPlateLock()), working);
    }
};

struct ClosePlateLock {
    /**
     * ClosePlateLock is M243 based on existing convention
     *
     * Acknowledged immediately upon receipt
     * */
    using ParseResult = std::optional<ClosePlateLock>;
    static constexpr auto prefix = std::array{'M', '2', '4', '3'};
    static constexpr const char* response = "M243 OK\n";

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt write_to_buf,
                                    InLimit write_to_limit) {
        return write_string_to_iterpair(write_to_buf, write_to_limit, response);
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
        return std::make_pair(ParseResult(ClosePlateLock()), working);
    }
};

struct GetPlateLockState {
    /*
    ** GetPlateLockState keys off a random gcode that sometimes does the right
    *thing since
    ** it's not like it's standardized or anything, M241
    ** Format: M241
    ** Example: M241
    */
    using ParseResult = std::optional<GetPlateLockState>;
    static constexpr auto prefix = std::array{'M', '2', '4', '1'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InLimit, InputIt>
    static auto write_response_into(InputIt write_to_buf,
                                    const InLimit write_to_limit,
                                    std::array<char, 14> plate_lock_state)
        -> InputIt {
        static constexpr const char* prefix = "M241 STATUS:";
        auto written =
            write_string_to_iterpair(write_to_buf, write_to_limit, prefix);
        if (written == write_to_limit) {
            return written;
        }
        written = write_string_to_iterpair(written, write_to_limit,
                                           plate_lock_state.begin());
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
        if (working != limit && !std::isspace(*working)) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetPlateLockState()), working);
    }
};

struct GetPlateLockStateDebug {
    /*
    ** GetPlateLockStateDebug keys off a random gcode and returns plate lock
    ** state and state of the open and closed plate lock optical switches
    ** Format: M241.D
    ** Example: M241.D
    */
    using ParseResult = std::optional<GetPlateLockStateDebug>;
    static constexpr auto prefix = std::array{'M', '2', '4', '1', '.', 'D'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InLimit, InputIt>
    static auto write_response_into(InputIt write_to_buf,
                                    const InLimit write_to_limit,
                                    std::array<char, 14> plate_lock_state,
                                    bool plate_lock_open_state,
                                    bool plate_lock_closed_state) -> InputIt {
        static constexpr const char* prefix = "M241.D STATUS:";
        auto written =
            write_string_to_iterpair(write_to_buf, write_to_limit, prefix);
        if (written == write_to_limit) {
            return written;
        }
        written = write_string_to_iterpair(written, write_to_limit,
                                           plate_lock_state.begin());
        if (written == write_to_limit) {
            return written;
        }
        static constexpr const char* open_sensor_prefix = " OpenSensor:";
        written = write_string_to_iterpair(written, write_to_limit,
                                           open_sensor_prefix);
        if (written == write_to_limit) {
            return written;
        }
        const char* open_state_chars = plate_lock_open_state ? "1" : "0";
        written =
            write_string_to_iterpair(written, write_to_limit, open_state_chars);
        if (written == write_to_limit) {
            return written;
        }
        static constexpr const char* closed_sensor_prefix = " ClosedSensor:";
        written = write_string_to_iterpair(written, write_to_limit,
                                           closed_sensor_prefix);
        if (written == write_to_limit) {
            return written;
        }
        const char* closed_state_chars = plate_lock_closed_state ? "1" : "0";
        written = write_string_to_iterpair(written, write_to_limit,
                                           closed_state_chars);
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
        return std::make_pair(ParseResult(GetPlateLockStateDebug()), working);
    }
};

/**
 * Uses M116, same as thermocyclers.
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
 * Uses M117, same as thermocyclers.
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
        auto res = snprintf(&*buf, (limit - buf), "M117 B:%0.4f C:%0.4f OK\n",
                            static_cast<float>(b), static_cast<float>(c));
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct DeactivateHeater {
    /**
     * DeactivateHeater is M106 based on existing convention
     *
     * Acknowledged immediately upon receipt
     * */
    using ParseResult = std::optional<DeactivateHeater>;
    static constexpr auto prefix = std::array{'M', '1', '0', '6'};
    static constexpr const char* response = "M106 OK\n";

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt write_to_buf,
                                    InLimit write_to_limit) {
        return write_string_to_iterpair(write_to_buf, write_to_limit, response);
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
        return std::make_pair(ParseResult(DeactivateHeater()), working);
    }
};

}  // namespace gcode
