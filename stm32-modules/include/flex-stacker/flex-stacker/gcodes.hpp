/*
** Definitions of valid gcodes understood by the flex-stacker; intended to work
** with the gcode parser in gcode_parser.hpp
*/

#pragma once

#pragma GCC push_options
#pragma GCC optimize("O0")

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
#include "flex-stacker/gcodes_motor.hpp"
#include "systemwide.h"

namespace gcode {

auto inline sensor_id_to_char(TOFSensorID sensor_id) -> char {
    return static_cast<char>(sensor_id == TOFSensorID::TOF_X ? 'X' : 'Z');
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

struct GetResetReason {
    /*
     * M114- GetResetReason retrieves the value of the RCC reset flag
     * that was captured at the beginning of the hardware setup
     * */
    using ParseResult = std::optional<GetResetReason>;
    static constexpr auto prefix = std::array{'M', '1', '1', '4'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, uint16_t reason)
        -> InputIt {
        int res = 0;
        // print a hexadecimal representation of the reset flags
        res = snprintf(&*buf, (limit - buf), "M114 R:%X OK\n", reason);
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
        return std::make_pair(ParseResult(GetResetReason()), working);
    }
};

struct SetSerialNumber {
    using ParseResult = std::optional<SetSerialNumber>;
    static constexpr auto prefix = std::array{'M', '9', '9', '6'};
    static constexpr const char* response = "M996 OK\n";

    struct SerialArg {
        static constexpr bool required = true;
        bool present = false;
        std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH> value = {' '};
    };

    std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH> value;

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
        auto res =
            gcode::SingleParser<SerialArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        if (!std::get<0>(arguments).present) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = SetSerialNumber{.value = std::get<0>(arguments).value};
        return std::make_pair(ret, res.second);
    }
};

struct GetDoorClosed {
    using ParseResult = std::optional<GetDoorClosed>;
    static constexpr auto prefix = std::array{'M', '1', '2', '2'};

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetDoorClosed()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, int door_closed)
        -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M122 D:%i OK\n", door_closed);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct SetStatusBarState {
    std::optional<StatusBarID> bar_id;
    std::optional<StatusBarColor> color;
    std::optional<StatusBarPattern> pattern;
    std::optional<uint32_t> duration;
    std::optional<int8_t> reps;
    float power;

    using ParseResult = std::optional<SetStatusBarState>;
    static constexpr auto prefix = std::array{'M', '2', '0', '0', ' '};
    static constexpr const char* response = "M200 OK\n";

    using PowerArg = Arg<float, 'P'>;
    using ColorArg = Arg<uint8_t, 'C'>;
    using KindArg = Arg<uint8_t, 'K'>;
    using PatternArg = Arg<uint8_t, 'A'>;
    using DurationArg = Arg<uint32_t, 'D'>;
    using RepsArg = Arg<int8_t, 'R'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<PowerArg, ColorArg, KindArg, PatternArg,
                                DurationArg, RepsArg>::parse_gcode(input, limit,
                                                                   prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetStatusBarState{.bar_id = std::nullopt,
                                     .color = std::nullopt,
                                     .pattern = std::nullopt,
                                     .duration = std::nullopt,
                                     .reps = std::nullopt,
                                     .power = 0};

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.power = static_cast<float>(std::get<0>(arguments).value);
        }
        if (std::get<1>(arguments).present) {
            ret.color =
                static_cast<StatusBarColor>(std::get<1>(arguments).value);
        }
        if (std::get<2>(arguments).present) {
            ret.bar_id = static_cast<StatusBarID>(std::get<2>(arguments).value);
        }
        if (std::get<3>(arguments).present) {
            ret.pattern =
                static_cast<StatusBarPattern>(std::get<3>(arguments).value);
        }
        if (std::get<4>(arguments).present) {
            ret.duration = static_cast<float>(std::get<4>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<5>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.reps = static_cast<int8_t>(std::get<5>(arguments).value);
        }
        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct GetTOFSensorStatus {
    TOFSensorID sensor_id;
    using ParseResult = std::optional<GetTOFSensorStatus>;
    static constexpr auto prefix = std::array{'M', '2', '1', '5'};

    using ArgX = ArgNoVal<'X'>;
    using ArgZ = ArgNoVal<'Z'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto sensor = TOFSensorID::TOF_X;
        auto res =
            gcode::SingleParser<ArgX, ArgZ>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        if (std::get<1>(arguments).present) {
            sensor = TOFSensorID::TOF_Z;
        } else if (!std::get<0>(arguments).present) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(
            ParseResult(GetTOFSensorStatus{.sensor_id = sensor}), res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    TOFSensorID sensor_id, bool ok,
                                    TOFSensorState state, TOFSensorMode mode)
        -> InputIt {
        char sensor_char = sensor_id == TOFSensorID::TOF_X ? 'X' : 'Z';
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M215 %c:%d T:%d M:%d OK\n",
                       sensor_char, ok, state, mode);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct GetTOFRegister {
    TOFSensorID sensor_id;
    uint8_t reg;

    using ParseResult = std::optional<GetTOFRegister>;
    static constexpr auto prefix = std::array{'M', '2', '2', '2', ' '};

    using XArg = Arg<uint8_t, 'X'>;
    using ZArg = Arg<uint8_t, 'Z'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<XArg, ZArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = GetTOFRegister{
            .sensor_id = TOFSensorID::TOF_X,
            .reg = 0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.reg = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.sensor_id = TOFSensorID::TOF_Z;
            ret.reg = std::get<1>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    TOFSensorID sensor_id, uint8_t reg,
                                    uint32_t data) -> InputIt {
        auto res = snprintf(&*buf, (limit - buf), "M222 %c:%u V:%lu OK\n",
                            sensor_id_to_char(sensor_id), reg, data);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct SetTOFRegister {
    TOFSensorID sensor_id;
    uint8_t reg;
    uint8_t data;

    using ParseResult = std::optional<SetTOFRegister>;
    static constexpr auto prefix = std::array{'M', '2', '2', '3', ' '};
    static constexpr const char* response = "M223 OK\n";

    using XArg = Arg<uint8_t, 'X'>;
    using ZArg = Arg<uint8_t, 'Z'>;
    using DataArg = ArgNoPrefix<uint32_t>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, DataArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetTOFRegister{
            .sensor_id = TOFSensorID::TOF_X,
            .reg = 0,
            .data = 0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.reg = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.sensor_id = TOFSensorID::TOF_Z;
            ret.reg = std::get<1>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<2>(arguments).present) {
            ret.data = std::get<2>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct EnableTOFSensor {
    TOFSensorID sensor_id;
    bool enable;

    using ParseResult = std::optional<EnableTOFSensor>;
    static constexpr auto prefix = std::array{'M', '2', '2', '4', ' '};
    static constexpr const char* response = "M224 OK\n";

    using XArg = Arg<uint8_t, 'X'>;
    using ZArg = Arg<uint8_t, 'Z'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<XArg, ZArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = EnableTOFSensor{
            .sensor_id = TOFSensorID::TOF_X,
            .enable = false,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.enable = static_cast<bool>(std::get<0>(arguments).value);
        } else if (std::get<1>(arguments).present) {
            ret.sensor_id = TOFSensorID::TOF_Z;
            ret.enable = static_cast<bool>(std::get<1>(arguments).value);
        } else {
            return std::make_pair(ParseResult(), input);
        }

        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct GetTOFHistogram {
    TOFSensorID sensor_id;

    using ParseResult = std::optional<GetTOFHistogram>;
    static constexpr auto prefix = std::array{'M', '2', '2', '5', ' '};

    using XArg = ArgNoVal<'X'>;
    using ZArg = ArgNoVal<'Z'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<XArg, ZArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = GetTOFHistogram{
            .sensor_id = TOFSensorID::TOF_X,
        };
        auto arguments = res.first.value();
        if (std::get<1>(arguments).present) {
            ret.sensor_id = TOFSensorID::TOF_Z;
        } else if (!std::get<0>(arguments).present) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    TOFSensorID sensor_id, uint8_t len,
                                    bool end, uint8_t* data) -> InputIt {
        // TODO: need to iterate through the data until `end == true`
        auto end_string = end ? "OK \n" : "";
        auto res = snprintf(&*buf, (limit - buf), "M225 %c D:%hhn %s",
                            sensor_id_to_char(sensor_id), data, end_string);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

}  // namespace gcode

#pragma GCC pop_options
