/*
** Messages that are specific to the motors.
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



struct MoveMotorAtFrequency {
    MotorID motor_id;
    bool direction;
    int32_t steps;
    uint32_t frequency;

    using ParseResult = std::optional<MoveMotorAtFrequency>;
    static constexpr auto prefix = std::array{'G', '6', ' '};
    static constexpr const char* response = "G6 OK\n";

    struct XArg {
        static constexpr auto prefix = std::array{'X'};
        static constexpr bool required = false;
        bool present = false;
    };
    struct ZArg {
        static constexpr auto prefix = std::array{'Z'};
        static constexpr bool required = false;
        bool present = false;
    };
    struct LArg {
        static constexpr auto prefix = std::array{'L'};
        static constexpr bool required = false;
        bool present = false;
    };
    struct DirArg {
        static constexpr bool required = true;
        bool present = false;
        bool value = false;
    };

    struct StepArg {
        static constexpr auto prefix = std::array{' ', 'S'};
        static constexpr bool required = true;
        bool present = false;
        int32_t value = 0;
    };
    struct FreqArg {
        static constexpr auto prefix = std::array{' ', 'F'};
        static constexpr bool required = false;
        bool present = false;
        uint32_t value = 0;
    };

    template <typename InputIt, typename Limit>
        requires std::forward_iterator<InputIt> &&
                 std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, LArg, StepArg, FreqArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = MoveMotorAtFrequency{
            .motor_id = MotorID::MOTOR_X,
            .direction = false,
            .steps = 0,
            .frequency = 0,
        };

        auto arguments = res.first.value();
        if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
        } else if (!std::get<0>(arguments).present) {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<3>(arguments).present) {
            ret.direction = std::get<3>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<4>(arguments).present) {
            ret.steps = std::get<4>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<4>(arguments).present) {
            ret.frequency = std::get<4>(arguments).value;
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

struct StopMotor {
        using ParseResult = std::optional<StopMotor>;
        static constexpr auto prefix = std::array{'M', '0'};
        static constexpr const char* response = "M0 OK\n";

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
            return std::make_pair(ParseResult(StopMotor()), working);
        }
};

}  // namespace gcode
