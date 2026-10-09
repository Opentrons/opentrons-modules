/*
** Definitions of valid gcodes understood by the reference-module; intended to
*work
** with the gcode parser in gcode_parser.hpp
*/

#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>

#include "core/gcode_parser.hpp"
#include "core/utility.hpp"
#include "incubator-module/errors.hpp"
#include "systemwide.h"

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

struct GetCapacitiveState {
    /*
     * M111- GetCapacitiveState gets the state of capacitive sensors (C1-C4)
     * */
    using ParseResult = std::optional<GetCapacitiveState>;
    static constexpr auto prefix = std::array{'M', '1', '1', '1'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double capacitive_ch1,
                                    double capacitive_ch2,
                                    double capacitive_ch3,
                                    double capacitive_ch4) -> InputIt {
        int res = 0;
        res = snprintf(
            &*buf, (limit - buf), "M111 C1:%.1f C2:%.1f C3:%.1f C4:%.1f OK\n",
            capacitive_ch1, capacitive_ch2, capacitive_ch3, capacitive_ch4);
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
        return std::make_pair(ParseResult(GetCapacitiveState()), working);
    }
};

struct GetProximityState {
    /*
     * M113- GetProximityState reads the GX-F6A sensor on PC2.
     * A value of 1 means the NPN output is sinking (object detected).
     * */
    using ParseResult = std::optional<GetProximityState>;
    static constexpr auto prefix = std::array{'M', '1', '1', '3'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    bool pc2_detected) -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf), "M113 PC2:%d OK\n",
                       pc2_detected ? 1 : 0);
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
        return std::make_pair(ParseResult(GetProximityState()), working);
    }
};

/*
** R-axis motor.
**   M17 [R]    enable
**   M18 [R]    disable
**   M0         stop
**   G0 R<mm> [V<mm/s>] [A<mm/s^2>] [D<mm/s>]
**   G0.S R<steps> F<steps/s> [A<steps/s^2>]
**   G28 R<dir> home on the inductive sensor; dir is 0 or 1
*/

template <typename ValueType, char... Chars>
struct MotorArg {
    static constexpr auto prefix = std::array{Chars...};
    static constexpr bool required = false;
    bool present = false;
    ValueType value = ValueType{};
};

template <typename ValueType, char... Chars>
struct RequiredMotorArg {
    static constexpr auto prefix = std::array{Chars...};
    static constexpr bool required = true;
    bool present = false;
    ValueType value = ValueType{};
};

template <char... Chars>
struct MotorFlag {
    static constexpr auto prefix = std::array{Chars...};
    static constexpr bool required = false;
    bool present = false;
};

struct EnableMotor {
    using ParseResult = std::optional<EnableMotor>;
    static constexpr auto prefix = std::array{'M', '1', '7'};
    static constexpr const char* response = "M17 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input ||
            (working != limit &&
             !std::isspace(static_cast<unsigned char>(*working)))) {
            return std::make_pair(ParseResult(), input);
        }
        auto res =
            SingleParser<MotorFlag<'R'>>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(EnableMotor()), res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct DisableMotor {
    using ParseResult = std::optional<DisableMotor>;
    static constexpr auto prefix = std::array{'M', '1', '8'};
    static constexpr const char* response = "M18 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input ||
            (working != limit &&
             !std::isspace(static_cast<unsigned char>(*working)))) {
            return std::make_pair(ParseResult(), input);
        }
        auto res =
            SingleParser<MotorFlag<'R'>>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(DisableMotor()), res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct StopMotor {
    using ParseResult = std::optional<StopMotor>;
    static constexpr auto prefix = std::array{'M', '0'};
    static constexpr const char* response = "M0 OK\n";

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input ||
            (working != limit &&
             !std::isspace(static_cast<unsigned char>(*working)))) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(StopMotor()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

struct MoveMotorInMm {
    float mm = 0;
    std::optional<float> mm_per_second = std::nullopt;
    std::optional<float> mm_per_second_sq = std::nullopt;
    std::optional<float> mm_per_second_discont = std::nullopt;

    using ParseResult = std::optional<MoveMotorInMm>;
    static constexpr auto prefix = std::array{'G', '0', ' '};
    static constexpr const char* response = "G0 OK\n";

    using DistanceArg = RequiredMotorArg<float, 'R'>;
    using VelArg = MotorArg<float, 'V'>;
    using AccelArg = MotorArg<float, 'A'>;
    using DiscontArg = MotorArg<float, 'D'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = SingleParser<DistanceArg, VelArg, AccelArg,
                                DiscontArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        auto ret = MoveMotorInMm{.mm = std::get<0>(arguments).value};
        if (std::get<1>(arguments).present) {
            ret.mm_per_second = std::get<1>(arguments).value;
        }
        if (std::get<2>(arguments).present) {
            ret.mm_per_second_sq = std::get<2>(arguments).value;
        }
        if (std::get<3>(arguments).present) {
            ret.mm_per_second_discont = std::get<3>(arguments).value;
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

struct MoveMotorInSteps {
    int32_t steps = 0;
    uint32_t steps_per_second = 0;
    uint32_t steps_per_second_sq = 0;

    using ParseResult = std::optional<MoveMotorInSteps>;
    static constexpr auto prefix = std::array{'G', '0', '.', 'S', ' '};
    static constexpr const char* response = "G0.S OK\n";

    using DistanceArg = RequiredMotorArg<int32_t, 'R'>;
    using FreqArg = RequiredMotorArg<uint32_t, 'F'>;
    using AccelArg = MotorArg<uint32_t, 'A'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = SingleParser<DistanceArg, FreqArg, AccelArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        auto ret = MoveMotorInSteps{
            .steps = std::get<0>(arguments).value,
            .steps_per_second = std::get<1>(arguments).value,
            .steps_per_second_sq = 0,
        };
        if (std::get<2>(arguments).present) {
            ret.steps_per_second_sq = std::get<2>(arguments).value;
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

struct HomeMotor {
    bool direction = false;

    using ParseResult = std::optional<HomeMotor>;
    static constexpr auto prefix = std::array{'G', '2', '8', ' '};
    static constexpr const char* response = "G28 OK\n";

    using DirectionArg = RequiredMotorArg<int, 'R'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            SingleParser<DirectionArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret =
            HomeMotor{.direction = std::get<0>(res.first.value()).value != 0};
        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

}  // namespace gcode
