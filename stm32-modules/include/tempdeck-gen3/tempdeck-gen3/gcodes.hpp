/**
 * @file gcodes.hpp
 * @brief Definitions of valid gcodes understood by tempdeck-gen3; intended
 * to work with the gcode parser in gcode_parser.hpp
 */

#pragma once

#include "core/gcode_parser.hpp"
#include "core/utility.hpp"
#include "systemwide.h"

namespace gcode {

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

struct EnterBootloader {
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

/**
 * @brief Command to turn off the thermal system. No parameters.
 *
 * M18\n
 *
 */
struct DeactivateAll {
    using ParseResult = std::optional<DeactivateAll>;
    static constexpr auto prefix = std::array{'M', '1', '8'};
    static constexpr const char* response = "M18 OK\n";

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
        return std::make_pair(ParseResult(DeactivateAll()), working);
    }
};

struct GetTemperatureDebug {
    using ParseResult = std::optional<GetTemperatureDebug>;
    static constexpr auto prefix = std::array{'M', '1', '0', '5', '.', 'D'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    float plate_temp, float heatsink_temp,
                                    uint16_t plate_adc, uint16_t heatsink_adc)
        -> InputIt {
        return buf + snprintf((char*)&*buf, std::distance(buf, limit),
                              "M105.D PT:%4.2f HST:%4.2f PA:%u HSA:%u OK\n",
                              plate_temp, heatsink_temp, plate_adc,
                              heatsink_adc);
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

/**
 * @brief SetTemperature is a command to set a temperature target for the
 * peltiers. There is one parameter, the target temp.
 *
 * M104 S[temp]\n
 *
 */
struct SetTemperature {
    using ParseResult = std::optional<SetTemperature>;
    static constexpr auto prefix = std::array{'M', '1', '0', '4'};
    static constexpr const char* response = "M104 OK\n";

    struct TargetArg {
        static constexpr auto prefix = std::array{'S'};
        static constexpr bool required = true;
        bool present = false;
        float value = 0.0F;
    };

    double target;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<TargetArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        auto ret = SetTemperature{.target = std::get<0>(arguments).value};
        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
    }
};

/**
 * @brief SetPeltierDebug is a command used to set the pulse width of the
 * peltiers on the Temp Deck. The only parameter is the power, which is
 * represented as a floating point value in the range [-1,1]. A value of
 * 0 will turn off the peltiers.
 *
 */
struct SetPeltierDebug {
    using ParseResult = std::optional<SetPeltierDebug>;
    static constexpr auto prefix = std::array{'M', '1', '0', '4', '.', 'D'};
    static constexpr const char* response = "M104.D OK\n";

    struct PowerArg {
        static constexpr auto prefix = std::array{'S'};
        static constexpr bool required = true;
        bool present = false;
        float value = 0.0F;
    };

    double power;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<PowerArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        if (!std::get<0>(arguments).present) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = SetPeltierDebug{.power = std::get<0>(arguments).value};
        return std::make_pair(ret, res.second);
    }

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit) -> InputIt {
        return write_string_to_iterpair(buf, limit, response);
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
    static constexpr auto prefix = std::array{'M', '1', '0', '6'};
    static constexpr const char* response = "M106 OK\n";
    static constexpr double min_power = 0.0F;
    static constexpr double max_power = 1.0F;

    struct PowerArg {
        static constexpr auto prefix = std::array{'S'};
        static constexpr bool required = true;
        bool present = false;
        float value = 0.0F;
    };

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
        auto res =
            gcode::SingleParser<PowerArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        if (!std::get<0>(arguments).present) {
            return std::make_pair(ParseResult(), input);
        }
        auto ret = SetFanManual{.power = std::get<0>(arguments).value};
        if (ret.power < min_power || ret.power > max_power) {
            return std::make_pair(ParseResult(), input);
        }
        return std::make_pair(ret, res.second);
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

struct SetPIDConstants {
    /**
     * SetPIDConstants uses M301. Sets the PWM of the fans as a percentage
     * between 0 and 1.
     *
     * M301 P[p] I[i] D[d]\n
     */
    using ParseResult = std::optional<SetPIDConstants>;
    static constexpr auto prefix = std::array{'M', '3', '0', '1'};
    static constexpr const char* response = "M301 OK\n";

    struct ArgP {
        static constexpr auto prefix = std::array{'P'};
        static constexpr bool required = true;
        bool present = false;
        float value = 0.0F;
    };
    struct ArgI {
        static constexpr auto prefix = std::array{'I'};
        static constexpr bool required = true;
        bool present = false;
        float value = 0.0F;
    };
    struct ArgD {
        static constexpr auto prefix = std::array{'D'};
        static constexpr bool required = true;
        bool present = false;
        float value = 0.0F;
    };

    double const_p, const_i, const_d;

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
        auto res = gcode::SingleParser<ArgP, ArgI, ArgD>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        auto ret = SetPIDConstants{.const_p = std::get<0>(arguments).value,
                                   .const_i = std::get<1>(arguments).value,
                                   .const_d = std::get<2>(arguments).value};
        return std::make_pair(ret, res.second);
    }
};

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
    static auto write_response_into(InputIt buf, InputLimit limit, double a,
                                    double b, double c) -> InputIt {
        auto res =
            snprintf(&*buf, (limit - buf), "M117 A:%0.4f B:%0.4f C:%0.4f OK\n",
                     static_cast<float>(a), static_cast<float>(b),
                     static_cast<float>(c));
        if (res <= 0) {
            return buf;
        }
        return buf + res;
    }
};

struct SetOffsetConstants {
    using ParseResult = std::optional<SetOffsetConstants>;
    static constexpr auto prefix = std::array{'M', '1', '1', '6'};
    static constexpr const char* response = "M116 OK\n";

    std::optional<double> const_a, const_b, const_c;

    struct ArgA {
        static constexpr auto prefix = std::array{'A'};
        static constexpr bool required = false;
        bool present = false;
        float value = 0.0F;
    };
    struct ArgB {
        static constexpr auto prefix = std::array{'B'};
        static constexpr bool required = false;
        bool present = false;
        float value = 0.0F;
    };
    struct ArgC {
        static constexpr auto prefix = std::array{'C'};
        static constexpr bool required = false;
        bool present = false;
        float value = 0.0F;
    };

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
        auto res = gcode::SingleParser<ArgA, ArgB, ArgC>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }
        auto arguments = res.first.value();
        auto ret = SetOffsetConstants{.const_a = std::nullopt,
                                      .const_b = std::nullopt,
                                      .const_c = std::nullopt};
        if (std::get<0>(arguments).present) {
            ret.const_a = std::get<0>(arguments).value;
        }
        if (std::get<1>(arguments).present) {
            ret.const_b = std::get<1>(arguments).value;
        }
        if (std::get<2>(arguments).present) {
            ret.const_c = std::get<2>(arguments).value;
        }
        return std::make_pair(ret, res.second);
    }
};

};  // namespace gcode
