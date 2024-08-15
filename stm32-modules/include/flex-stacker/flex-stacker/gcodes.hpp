/*
** Definitions of valid gcodes understood by the flex-stacker; intended to work
** with the gcode parser in gcode_parser.hpp
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

#include "core/gcode_parser.hpp"
#include "core/utility.hpp"
#include "flex-stacker/errors.hpp"
#include "systemwide.h"

namespace gcode {

auto inline motor_id_to_char(MotorID motor_id) -> const char* {
    return static_cast<const char*>(motor_id == MotorID::MOTOR_X   ? "X"
                                    : motor_id == MotorID::MOTOR_Z ? "Z"
                                                                   : "L");
}

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

struct GetBoardRevision {
    using ParseResult = std::optional<GetBoardRevision>;
    static constexpr auto prefix = std::array{'M', '9', '0', '0', '.', 'D'};

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
    static auto write_response_into(InputIt buf, InLimit limit, int revision)
        -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M900.D C:%i OK\n", revision);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct GetTMCRegister {
    MotorID motor_id;
    uint8_t reg;

    using ParseResult = std::optional<GetTMCRegister>;
    static constexpr auto prefix = std::array{'M', '9', '2', '0', ' '};

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        MotorID motor_id_val = MotorID::MOTOR_X;
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        switch (*working) {
            case 'X':
                motor_id_val = MotorID::MOTOR_X;
                break;
            case 'Z':
                motor_id_val = MotorID::MOTOR_Z;
                break;
            case 'L':
                motor_id_val = MotorID::MOTOR_L;
                break;
            default:
                return std::make_pair(ParseResult(), input);
        }
        std::advance(working, 1);
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }

        auto reg_res = gcode::parse_value<uint8_t>(working, limit);
        if (!reg_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(
            ParseResult(GetTMCRegister{.motor_id = motor_id_val,
                                       .reg = reg_res.first.value()}),
            reg_res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    MotorID motor_id, uint8_t reg,
                                    uint32_t data) -> InputIt {
        auto res = snprintf(&*buf, (limit - buf), "M920 %s%u %lu OK\n",
                            motor_id_to_char(motor_id), reg, data);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct SetTMCRegister {
    MotorID motor_id;
    uint8_t reg;
    uint32_t data;

    using ParseResult = std::optional<SetTMCRegister>;
    static constexpr auto prefix = std::array{'M', '9', '2', '1', ' '};
    static constexpr const char* response = "M921 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        MotorID motor_id_val = MotorID::MOTOR_X;
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        switch (*working) {
            case 'X':
                motor_id_val = MotorID::MOTOR_X;
                break;
            case 'Z':
                motor_id_val = MotorID::MOTOR_Z;
                break;
            case 'L':
                motor_id_val = MotorID::MOTOR_L;
                break;
            default:
                return std::make_pair(ParseResult(), input);
        }
        std::advance(working, 1);
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }

        auto reg_res = gcode::parse_value<uint8_t>(working, limit);
        if (!reg_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        std::advance(working, 1);
        if (working == limit) {
            return std::make_pair(ParseResult(), input);
        }

        auto data_res = gcode::parse_value<uint32_t>(working, limit);
        if (!data_res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        return std::make_pair(
            ParseResult(SetTMCRegister{.motor_id = motor_id_val,
                                       .reg = reg_res.first.value(),
                                       .data = data_res.first.value()}),
            data_res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct ArgX {
    static constexpr auto prefix = std::array{'X'};
    static constexpr bool required = false;
    bool present = false;
};
struct ArgZ {
    static constexpr auto prefix = std::array{'Z'};
    static constexpr bool required = false;
    bool present = false;
};
struct ArgL {
    static constexpr auto prefix = std::array{'L'};
    static constexpr bool required = false;
    bool present = false;
};

struct EnableMotor {
    std::optional<bool> x, z, l;

    using ParseResult = std::optional<EnableMotor>;
    static constexpr auto prefix = std::array{'M', '1', '7', ' '};
    static constexpr const char* response = "M17 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        // parse for motor engage
        std::optional<bool> x;
        std::optional<bool> z;
        std::optional<bool> l;
        auto res = gcode::SingleParser<ArgX, ArgZ, ArgL>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            x = true;
        }
        if (std::get<1>(arguments).present) {
            z = true;
        }
        if (std::get<2>(arguments).present) {
            l = true;
        }
        return std::make_pair(EnableMotor{.x = x, .z = z, .l = l}, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct DisableMotor {
    std::optional<bool> x, z, l;

    using ParseResult = std::optional<DisableMotor>;
    static constexpr auto prefix = std::array{'M', '1', '8', ' '};
    static constexpr const char* response = "M18 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        std::optional<bool> x;
        std::optional<bool> z;
        std::optional<bool> l;
        auto res = gcode::SingleParser<ArgX, ArgZ, ArgL>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            x = false;
        }
        if (std::get<1>(arguments).present) {
            z = false;
        }
        if (std::get<2>(arguments).present) {
            l = false;
        }
        return std::make_pair(DisableMotor{.x = x, .z = z, .l = l}, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};
}  // namespace gcode
