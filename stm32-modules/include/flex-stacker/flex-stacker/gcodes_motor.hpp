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

template <typename ValueType, char... Chars>
struct Arg {
    static constexpr auto prefix = std::array{Chars...};
    static constexpr bool required = false;
    bool present = false;
    ValueType value = ValueType{};
};

template <char... Chars>
struct ArgNoVal {
    static constexpr auto prefix = std::array{Chars...};
    static constexpr bool required = false;
    bool present = false;
};

template <typename ValueType>
struct ArgNoPrefix {
    static constexpr bool required = false;
    bool present = false;
    ValueType value = ValueType{};
};

struct StallGuardResult {
    uint32_t data;

    using ParseResult = std::optional<StallGuardResult>;
    static constexpr auto prefix = std::array{'M', '9', '0', '0', ' '};

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(StallGuardResult()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, uint32_t data)
        -> InputIt {
        auto res = snprintf(&*buf, (limit - buf), "M900 %lu OK\n", data);
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

    using XArg = Arg<uint8_t, 'X'>;
    using ZArg = Arg<uint8_t, 'Z'>;
    using LArg = Arg<uint8_t, 'L'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, LArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = GetTMCRegister{
            .motor_id = MotorID::MOTOR_X,
            .reg = 0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.reg = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.reg = std::get<1>(arguments).value;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.reg = std::get<2>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ret, res.second);
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

struct SetMicrosteps {
    MotorID motor_id;
    uint8_t microsteps_power;

    using ParseResult = std::optional<SetMicrosteps>;
    static constexpr auto prefix = std::array{'M', '9', '0', '9', ' '};
    static constexpr const char* response = "M909 OK\n";

    using XArg = Arg<uint8_t, 'X'>;
    using ZArg = Arg<uint8_t, 'Z'>;
    using LArg = Arg<uint8_t, 'L'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, LArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret =
            SetMicrosteps{.motor_id = MotorID::MOTOR_X, .microsteps_power = 0};

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.microsteps_power = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.microsteps_power = std::get<1>(arguments).value;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.microsteps_power = std::get<2>(arguments).value;
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

struct SetTMCRegister {
    MotorID motor_id;
    uint8_t reg;
    uint32_t data;

    using ParseResult = std::optional<SetTMCRegister>;
    static constexpr auto prefix = std::array{'M', '9', '2', '1', ' '};
    static constexpr const char* response = "M921 OK\n";

    using XArg = Arg<uint8_t, 'X'>;
    using ZArg = Arg<uint8_t, 'Z'>;
    using LArg = Arg<uint8_t, 'L'>;
    using DataArg = ArgNoPrefix<uint32_t>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, LArg, DataArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetTMCRegister{
            .motor_id = MotorID::MOTOR_X,
            .reg = 0,
            .data = 0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.reg = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.reg = std::get<1>(arguments).value;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.reg = std::get<2>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<3>(arguments).present) {
            ret.data = std::get<3>(arguments).value;
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

struct SetRunCurrent {
    MotorID motor_id;
    float current;

    using ParseResult = std::optional<SetRunCurrent>;
    static constexpr auto prefix = std::array{'M', '9', '0', '6', ' '};
    static constexpr const char* response = "M906 OK\n";
    using XArg = Arg<float, 'X'>;
    using ZArg = Arg<float, 'Z'>;
    using LArg = Arg<float, 'L'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, LArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = SetRunCurrent{
            .motor_id = MotorID::MOTOR_X,
            .current = 0.0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.current = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.current = std::get<1>(arguments).value;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.current = std::get<2>(arguments).value;
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

struct SetHoldCurrent {
    MotorID motor_id;
    float current;

    using ParseResult = std::optional<SetHoldCurrent>;
    static constexpr auto prefix = std::array{'M', '9', '0', '7', ' '};
    static constexpr const char* response = "M907 OK\n";
    using XArg = Arg<float, 'X'>;
    using ZArg = Arg<float, 'Z'>;
    using LArg = Arg<float, 'L'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, LArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = SetHoldCurrent{
            .motor_id = MotorID::MOTOR_X,
            .current = 0.0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.current = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.current = std::get<1>(arguments).value;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.current = std::get<2>(arguments).value;
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

struct EnableMotor {
    std::optional<bool> x, z, l;

    using ParseResult = std::optional<EnableMotor>;
    static constexpr auto prefix = std::array{'M', '1', '7', ' '};
    static constexpr const char* response = "M17 OK\n";

    using ArgX = ArgNoVal<'X'>;
    using ArgZ = ArgNoVal<'Z'>;
    using ArgL = ArgNoVal<'L'>;

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

    using ArgX = ArgNoVal<'X'>;
    using ArgZ = ArgNoVal<'Z'>;
    using ArgL = ArgNoVal<'L'>;

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

struct MoveMotorInSteps {
    MotorID motor_id;
    int32_t steps;
    uint32_t steps_per_second;
    uint32_t steps_per_second_sq;

    using ParseResult = std::optional<MoveMotorInSteps>;
    static constexpr auto prefix = std::array{'G', '0', '.', 'S', ' '};
    static constexpr const char* response = "G0.S OK\n";

    using XArg = Arg<int32_t, 'X'>;
    using ZArg = Arg<int32_t, 'Z'>;
    using LArg = Arg<int32_t, 'L'>;
    using FreqArg = Arg<uint32_t, 'F'>;
    using RampArg = Arg<uint32_t, 'R'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<XArg, ZArg, LArg, FreqArg,
                                RampArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = MoveMotorInSteps{
            .motor_id = MotorID::MOTOR_X,
            .steps = 0,
            .steps_per_second = 0,
            .steps_per_second_sq = 0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.steps = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.steps = std::get<1>(arguments).value;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.steps = std::get<2>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<3>(arguments).present) {
            ret.steps_per_second = std::get<3>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<4>(arguments).present) {
            ret.steps_per_second_sq = std::get<4>(arguments).value;
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

struct MoveMotorInMm {
    MotorID motor_id;
    float mm;
    std::optional<float> mm_per_second, mm_per_second_sq, mm_per_second_discont;

    using ParseResult = std::optional<MoveMotorInMm>;
    static constexpr auto prefix = std::array{'G', '0', ' '};
    static constexpr const char* response = "G0 OK\n";

    using XArg = Arg<float, 'X'>;
    using ZArg = Arg<float, 'Z'>;
    using LArg = Arg<float, 'L'>;
    using VelArg = Arg<float, 'V'>;
    using AccelArg = Arg<float, 'A'>;
    using DiscontArg = Arg<float, 'D'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<XArg, ZArg, LArg, VelArg, AccelArg,
                                DiscontArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = MoveMotorInMm{
            .motor_id = MotorID::MOTOR_X,
            .mm = 0.0,
            .mm_per_second = std::nullopt,
            .mm_per_second_sq = std::nullopt,
            .mm_per_second_discont = std::nullopt,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.mm = std::get<0>(arguments).value;
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.mm = std::get<1>(arguments).value;
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.mm = std::get<2>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<3>(arguments).present) {
            ret.mm_per_second = std::get<3>(arguments).value;
        }

        if (std::get<4>(arguments).present) {
            ret.mm_per_second_sq = std::get<4>(arguments).value;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<5>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.mm_per_second_discont = std::get<5>(arguments).value;
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

struct MoveToLimitSwitch {
    MotorID motor_id;
    bool direction;
    std::optional<float> mm_per_second, mm_per_second_sq, mm_per_second_discont;

    using ParseResult = std::optional<MoveToLimitSwitch>;
    static constexpr auto prefix = std::array{'G', '5', ' '};
    static constexpr const char* response = "G5 OK\n";

    using XArg = Arg<int, 'X'>;
    using ZArg = Arg<int, 'Z'>;
    using LArg = Arg<int, 'L'>;
    using VelArg = Arg<float, 'V'>;
    using AccelArg = Arg<float, 'A'>;
    using DiscontArg = Arg<float, 'D'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<XArg, ZArg, LArg, VelArg, AccelArg,
                                DiscontArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = MoveToLimitSwitch{
            .motor_id = MotorID::MOTOR_X,
            .direction = false,
            .mm_per_second = std::nullopt,
            .mm_per_second_sq = std::nullopt,
            .mm_per_second_discont = std::nullopt,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.direction = static_cast<bool>(std::get<0>(arguments).value);
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.direction = static_cast<bool>(std::get<1>(arguments).value);
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.direction = static_cast<bool>(std::get<2>(arguments).value);
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<3>(arguments).present) {
            ret.mm_per_second = std::get<3>(arguments).value;
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<4>(arguments).present) {
            ret.mm_per_second_sq = std::get<4>(arguments).value;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<5>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.mm_per_second_discont = std::get<5>(arguments).value;
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

struct MoveMotor {
    MotorID motor_id;
    bool direction;
    uint32_t frequency;

    using ParseResult = std::optional<MoveMotor>;
    static constexpr auto prefix = std::array{'G', '5', ' '};
    static constexpr const char* response = "G5 OK\n";

    using XArg = Arg<int, 'X'>;
    using ZArg = Arg<int, 'Z'>;
    using LArg = Arg<int, 'L'>;
    using FreqArg = Arg<uint32_t, 'F'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<XArg, ZArg, LArg, FreqArg>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = MoveMotor{
            .motor_id = MotorID::MOTOR_X,
            .direction = false,
            .frequency = 0,
        };

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.direction = static_cast<bool>(std::get<0>(arguments).value);
        } else if (std::get<1>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_Z;
            ret.direction = static_cast<bool>(std::get<1>(arguments).value);
        } else if (std::get<2>(arguments).present) {
            ret.motor_id = MotorID::MOTOR_L;
            ret.direction = static_cast<bool>(std::get<2>(arguments).value);
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<3>(arguments).present) {
            ret.frequency = std::get<3>(arguments).value;
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

struct GetLimitSwitches {
    using ParseResult = std::optional<GetLimitSwitches>;
    static constexpr auto prefix = std::array{'M', '1', '1', '9'};

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto working = prefix_matches(input, limit, prefix);
        if (working == input) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetLimitSwitches()), working);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, int x_extended,
                                    int x_retracted, int z_extended,
                                    int z_retracted, int l_released, int l_held)
        -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf),
                       "M119 XE:%i XR:%i ZE:%i ZR:%i LR:%i LH:%i OK\n",
                       x_extended, x_retracted, z_extended, z_retracted,
                       l_released, l_held);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct GetMoveParams {
    MotorID motor_id;
    using ParseResult = std::optional<GetMoveParams>;
    static constexpr auto prefix = std::array{'M', '1', '2', '0'};

    using ArgX = ArgNoVal<'X'>;
    using ArgZ = ArgNoVal<'Z'>;
    using ArgL = ArgNoVal<'L'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        MotorID motor = MotorID::MOTOR_X;
        auto res = gcode::SingleParser<ArgX, ArgZ, ArgL>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        if (std::get<1>(arguments).present) {
            motor = MotorID::MOTOR_Z;
        } else if (std::get<2>(arguments).present) {
            motor = MotorID::MOTOR_L;
        } else if (!std::get<0>(arguments).present) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ParseResult(GetMoveParams{.motor_id = motor}),
                              res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    MotorID motor_id, float velocity,
                                    float accel, float velocity_discont)
        -> InputIt {
        char motor_char = motor_id == MotorID::MOTOR_X   ? 'X'
                          : motor_id == MotorID::MOTOR_Z ? 'Z'
                                                         : 'L';
        int res = 0;
        res =
            snprintf(&*buf, (limit - buf), "M120 %c V:%.3f A:%.3f D:%.3f OK\n",
                     motor_char, velocity, accel, velocity_discont);
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

}  // namespace gcode
