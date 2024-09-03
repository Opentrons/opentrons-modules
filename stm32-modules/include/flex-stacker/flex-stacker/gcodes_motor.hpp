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

struct MoveMotorInSteps {
    MotorID motor_id;
    int32_t steps;
    uint32_t steps_per_second;
    uint32_t steps_per_second_sq;

    using ParseResult = std::optional<MoveMotorInSteps>;
    static constexpr auto prefix = std::array{'G', '0', '.', 'S', ' '};
    static constexpr const char* response = "G0.S OK\n";

    struct XArg {
        static constexpr auto prefix = std::array{'X'};
        static constexpr bool required = false;
        bool present = false;
        int32_t value = 0;
    };
    struct ZArg {
        static constexpr auto prefix = std::array{'Z'};
        static constexpr bool required = false;
        bool present = false;
        int32_t value = 0;
    };
    struct LArg {
        static constexpr auto prefix = std::array{'L'};
        static constexpr bool required = false;
        bool present = false;
        int32_t value = 0;
    };
    struct FreqArg {
        static constexpr auto prefix = std::array{'F'};
        static constexpr bool required = true;
        bool present = false;
        uint32_t value = 0;
    };
    struct RampArg {
        static constexpr auto prefix = std::array{'R'};
        static constexpr bool required = false;
        bool present = false;
        uint32_t value = 0;
    };

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
    int32_t mm;
    uint32_t mm_per_second;
    uint32_t mm_per_second_sq;
    uint32_t mm_per_second_discont;

    using ParseResult = std::optional<MoveMotorInMm>;
    static constexpr auto prefix = std::array{'G', '0', ' '};
    static constexpr const char* response = "G0 OK\n";

    struct XArg {
        static constexpr auto prefix = std::array{'X'};
        static constexpr bool required = false;
        bool present = false;
        int32_t value = 0;
    };
    struct ZArg {
        static constexpr auto prefix = std::array{'Z'};
        static constexpr bool required = false;
        bool present = false;
        int32_t value = 0;
    };
    struct LArg {
        static constexpr auto prefix = std::array{'L'};
        static constexpr bool required = false;
        bool present = false;
        int32_t value = 0;
    };
    struct VelArg {
        static constexpr auto prefix = std::array{'V'};
        static constexpr bool required = true;
        bool present = false;
        uint32_t value = 0;
    };

    struct AccelArg {
        static constexpr auto prefix = std::array{'A'};
        static constexpr bool required = false;
        bool present = false;
        uint32_t value = 0;
    };

    struct DiscontArg {
        static constexpr auto prefix = std::array{'D'};
        static constexpr bool required = false;
        bool present = false;
        uint32_t value = 0;
    };

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
            .mm = 0,
            .mm_per_second = 0,
            .mm_per_second_sq = 0,
            .mm_per_second_discont = 0,
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
        } else {
            return std::make_pair(ParseResult(), input);
        }

        if (std::get<4>(arguments).present) {
            ret.mm_per_second_sq = std::get<4>(arguments).value;
        }
        if (std::get<5>(arguments).present) {
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
    uint32_t mm_per_second;
    uint32_t mm_per_second_sq;
    uint32_t mm_per_second_discont;

    using ParseResult = std::optional<MoveToLimitSwitch>;
    static constexpr auto prefix = std::array{'G', '5', ' '};
    static constexpr const char* response = "G5 OK\n";

    struct XArg {
        static constexpr auto prefix = std::array{'X'};
        static constexpr bool required = false;
        bool present = false;
        int value = 0;
    };
    struct ZArg {
        static constexpr auto prefix = std::array{'Z'};
        static constexpr bool required = false;
        bool present = false;
        int value = 0;
    };
    struct LArg {
        static constexpr auto prefix = std::array{'L'};
        static constexpr bool required = false;
        bool present = false;
        int value = 0;
    };
    struct VelArg {
        static constexpr auto prefix = std::array{'V'};
        static constexpr bool required = true;
        bool present = false;
        uint32_t value = 0;
    };
    struct AccelArg {
        static constexpr auto prefix = std::array{'A'};
        static constexpr bool required = false;
        bool present = false;
        uint32_t value = 0;
    };
    struct DiscontArg {
        static constexpr auto prefix = std::array{'D'};
        static constexpr bool required = false;
        bool present = false;
        uint32_t value = 0;
    };

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
            .mm_per_second = 0,
            .mm_per_second_sq = 0,
            .mm_per_second_discont = 0,
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
        if (std::get<5>(arguments).present) {
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

    struct XArg {
        static constexpr auto prefix = std::array{'X'};
        static constexpr bool required = false;
        bool present = false;
        int value = 0;
    };
    struct ZArg {
        static constexpr auto prefix = std::array{'Z'};
        static constexpr bool required = false;
        bool present = false;
        int value = 0;
    };
    struct LArg {
        static constexpr auto prefix = std::array{'L'};
        static constexpr bool required = false;
        bool present = false;
        int value = 0;
    };
    struct FreqArg {
        static constexpr auto prefix = std::array{'F'};
        static constexpr bool required = false;
        bool present = false;
        uint32_t value = 0;
    };

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

}  // namespace gcode
