/*
** Definitions of valid gcodes understood by the vacuum-module; intended to
*work with the gcode parser in gcode_parser.hpp
*/

#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>

#include "core/gcode_parser.hpp"
#include "core/utility.hpp"
#include "systemwide.h"
#include "vacuum-module/errors.hpp"

namespace gcode {

template <typename ValueType, char... Chars>
struct Arg {
    static constexpr auto prefix = std::array{Chars...};
    static constexpr bool required = false;
    bool present = false;
    ValueType value = ValueType{};
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

struct SetPressureState {
    /*
     * M120- SetPressureState set state of pressure control (target pressure,
     * current pressure)
     * */
    double pressure = 0;
    uint32_t duration_s = 0;
    uint32_t timeout_s = 0;
    double ramp_rate = 0;
    bool vent_after = true;
    bool start_pump = false;

    using ParseResult = std::optional<SetPressureState>;
    static constexpr auto prefix = std::array{'M', '1', '2', '0', ' '};
    static constexpr const char* response = "M120 OK\n";

    using StartArg = Arg<uint8_t, 'S'>;
    using PressureArg = Arg<float, 'P'>;
    using DurationArg = Arg<uint32_t, 'D'>;
    using TimeoutArg = Arg<uint32_t, 'T'>;
    using RampArg = Arg<float, 'R'>;
    using VentArg = Arg<uint8_t, 'V'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<StartArg, PressureArg, DurationArg, TimeoutArg,
                                RampArg, VentArg>::parse_gcode(input, limit,
                                                               prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetPressureState{.pressure = 0.0,
                                    .duration_s = 0,
                                    .timeout_s = 0,
                                    .ramp_rate = 0.0,
                                    .vent_after = true,
                                    .start_pump = false};

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.start_pump = static_cast<bool>(std::get<0>(arguments).value);
        }
        if (std::get<1>(arguments).present) {
            ret.pressure = static_cast<double>(std::get<1>(arguments).value);
        }
        if (std::get<2>(arguments).present) {
            ret.duration_s =
                static_cast<uint32_t>(std::get<2>(arguments).value);
        }
        if (std::get<3>(arguments).present) {
            ret.timeout_s = static_cast<uint32_t>(std::get<3>(arguments).value);
        }
        if (std::get<4>(arguments).present) {
            ret.ramp_rate = static_cast<double>(std::get<4>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<5>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.vent_after = static_cast<bool>(std::get<5>(arguments).value);
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

struct GetPressureState {
    /*
     * M121- GetPressureState get state of pressure control (target pressure,
     * current pressure)
     * */
    using ParseResult = std::optional<GetPressureState>;
    static constexpr auto prefix = std::array{'M', '1', '2', '1'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double target_pressure,
                                    double current_pressure,
                                    double pressure_abs_a,
                                    double pressure_abs_b, double pressure_atm,
                                    bool vacuum_enabled,
                                    bool target_pressure_reached,
                                    uint32_t duration_s,
                                    VentState vent_state) -> InputIt {
        int res = 0;
        res = snprintf(
            &*buf, (limit - buf),
            "M121 T:%.1f C:%.1f A:%.1f B:%.1f H:%.1f E:%d R:%d D:%ld V:%d OK\n",
            target_pressure, current_pressure, pressure_abs_a, pressure_abs_b,
            pressure_atm, vacuum_enabled, target_pressure_reached, duration_s,
            vent_state);
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
        return std::make_pair(ParseResult(GetPressureState()), working);
    }
};

struct SetPumpState {
    /*
     * M122- SetPumpState set state of pump control (start pump, target rpm,
     * on/off). Supports E:<duration_s> T:<timeout_s> (D: is duty) to enable
     * duration tracking and waste detection via notification to PressureTask.
     * */
    double target_rpm = 0;
    uint8_t duty_cycle = 0;
    bool start_pump = false;
    uint32_t duration_s = 0;
    uint32_t timeout_s = 0;
    double ramp_rate = 0;
    bool vent_after = true;

    using ParseResult = std::optional<SetPumpState>;
    static constexpr auto prefix = std::array{'M', '1', '2', '2', ' '};
    static constexpr const char* response = "M122 OK\n";

    using StartArg = Arg<uint8_t, 'S'>;
    using RPMArg = Arg<uint16_t, 'R'>;
    using DutyArg = Arg<uint8_t, 'D'>;
    using DurationArg = Arg<uint32_t, 'E'>;
    using TimeoutArg = Arg<uint32_t, 'T'>;
    using RampArg = Arg<float, 'A'>;
    using VentArg = Arg<uint8_t, 'V'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<StartArg, RPMArg, DutyArg, DurationArg,
                                TimeoutArg, RampArg,
                                VentArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetPumpState{.target_rpm = 0,
                                .duty_cycle = 0,
                                .start_pump = false,
                                .duration_s = 0,
                                .timeout_s = 0,
                                .ramp_rate = 0.0,
                                .vent_after = true};

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.start_pump = static_cast<bool>(std::get<0>(arguments).value);
        }
        if (std::get<1>(arguments).present) {
            ret.target_rpm = static_cast<double>(std::get<1>(arguments).value);
        }
        if (std::get<2>(arguments).present) {
            ret.duty_cycle = static_cast<uint8_t>(std::get<2>(arguments).value);
        }
        if (std::get<3>(arguments).present) {
            ret.duration_s =
                static_cast<uint32_t>(std::get<3>(arguments).value);
        }
        if (std::get<4>(arguments).present) {
            ret.timeout_s = static_cast<uint32_t>(std::get<4>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<5>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.ramp_rate = static_cast<double>(std::get<5>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<6>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.vent_after = static_cast<bool>(std::get<6>(arguments).value);
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

struct GetPumpState {
    /*
     * M123- GetPumpState state of the pump (target rpm, current rpm, etc)
     * */
    using ParseResult = std::optional<GetPumpState>;
    static constexpr auto prefix = std::array{'M', '1', '2', '3'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit,
                                    double target_rpm, double current_rpm,
                                    uint8_t target_pwm, uint8_t current_pwm,
                                    bool pump_running, bool manual_control)
        -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf),
                       "M123 T:%.1f R:%.1f A:%d D:%d E:%d M:%d OK\n",
                       target_rpm, current_rpm, target_pwm, current_pwm,
                       pump_running, manual_control);
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
        return std::make_pair(ParseResult(GetPumpState()), working);
    }
};

struct SetVentState {
    /*
     * M124- SetVentState set state of vent (on/off)
     * */
    VentState state;

    using ParseResult = std::optional<SetVentState>;
    static constexpr auto prefix = std::array{'M', '1', '2', '4', ' '};
    static constexpr const char* response = "M124 OK\n";

    using VentArg = Arg<uint8_t, 'V'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<VentArg>::parse_gcode(input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetVentState{.state = VentState::CLOSED};
        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.state = static_cast<VentState>(std::get<0>(arguments).value);
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

struct SetPressurePID {
    /*
     * M125- SetPressurePID tune the pressure regulation parameters
     * */
    std::optional<double> kp;
    std::optional<double> ki;
    std::optional<double> kd;
    std::optional<double> overshoot;
    std::optional<double> k_velocity;
    std::optional<double> k_holding;
    std::optional<double> rel_tol_pct;
    bool reset = false;

    using ParseResult = std::optional<SetPressurePID>;
    static constexpr auto prefix = std::array{'M', '1', '2', '5', ' '};
    static constexpr const char* response = "M125 OK\n";

    using P = Arg<float, 'P'>;
    using I = Arg<float, 'I'>;
    using D = Arg<float, 'D'>;
    using O = Arg<float, 'O'>;
    using V = Arg<float, 'V'>;
    using H = Arg<float, 'H'>;
    using T = Arg<float, 'T'>;
    using R = Arg<uint8_t, 'R'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res = gcode::SingleParser<P, I, D, O, V, H, T, R>::parse_gcode(
            input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetPressurePID{.reset = false};

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.kp = static_cast<double>(std::get<0>(arguments).value);
        }
        if (std::get<1>(arguments).present) {
            ret.ki = static_cast<double>(std::get<1>(arguments).value);
        }
        if (std::get<2>(arguments).present) {
            ret.kd = static_cast<double>(std::get<2>(arguments).value);
        }
        if (std::get<3>(arguments).present) {
            ret.overshoot = static_cast<double>(std::get<3>(arguments).value);
        }
        if (std::get<4>(arguments).present) {
            ret.k_velocity = static_cast<double>(std::get<4>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<5>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.k_holding = static_cast<double>(std::get<5>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<6>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.rel_tol_pct = static_cast<double>(std::get<6>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<7>(arguments).present) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            ret.reset = static_cast<bool>(std::get<7>(arguments).value);
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

struct GetPressurePID {
    /*
     * M126- GetPressurePID get the pressure control PID tunings
     * */
    using ParseResult = std::optional<GetPressurePID>;
    static constexpr auto prefix = std::array{'M', '1', '2', '6'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(InputIt buf, InLimit limit, double kp,
                                    double ki, double kd, double overshoot,
                                    double k_velocity, double k_holding,
                                    double rel_tol_pct) -> InputIt {
        int res = 0;
        res = snprintf(
            &*buf, (limit - buf),
            "M126 P:%.1f I:%.1f D:%.1f O:%.1f V:%.1f H:%.1f T:%.2f OK\n", kp,
            ki, kd, overshoot, k_velocity, k_holding, rel_tol_pct);
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
        return std::make_pair(ParseResult(GetPressurePID()), working);
    }
};

struct SetWasteDetectionConfig {
    /*
     * M127- SetWasteDetectionConfig configure the waste full detection feature
     * */
    std::optional<double> p_window_start = std::nullopt;
    std::optional<double> p_window_end = std::nullopt;
    std::optional<double> baseline_fast_factor = std::nullopt;
    std::optional<double> max_delta_per_tick = std::nullopt;
    std::optional<double> max_rise_per_tick = std::nullopt;
    std::optional<double> max_cummulative_rise = std::nullopt;
    std::optional<double> p_filter_alpha = std::nullopt;
    std::optional<double> min_window_time = std::nullopt;
    std::optional<double> max_window_time = std::nullopt;
    bool enable_waste_full = true;

    using ParseResult = std::optional<SetWasteDetectionConfig>;
    static constexpr auto prefix = std::array{'M', '1', '2', '7', ' '};
    static constexpr const char* response = "M127 OK\n";

    using S = Arg<float, 'S'>;
    using P = Arg<float, 'P'>;
    using F = Arg<float, 'F'>;
    using D = Arg<float, 'D'>;
    using R = Arg<float, 'R'>;
    using C = Arg<float, 'C'>;
    using A = Arg<float, 'A'>;
    using M = Arg<float, 'M'>;
    using X = Arg<float, 'X'>;
    using E = Arg<uint8_t, 'E'>;

    template <typename InputIt, typename Limit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<Limit, InputIt>
    static auto parse(const InputIt& input, Limit limit)
        -> std::pair<ParseResult, InputIt> {
        auto res =
            gcode::SingleParser<S, P, F, D, R, C, A, M, X, E>::parse_gcode(
                input, limit, prefix);
        if (!res.first.has_value()) {
            return std::make_pair(ParseResult(), input);
        }

        auto ret = SetWasteDetectionConfig{.enable_waste_full = true};

        auto arguments = res.first.value();
        if (std::get<0>(arguments).present) {
            ret.p_window_start =
                static_cast<double>(std::get<0>(arguments).value);
        }
        if (std::get<1>(arguments).present) {
            ret.p_window_end =
                static_cast<double>(std::get<1>(arguments).value);
        }
        if (std::get<2>(arguments).present) {
            ret.baseline_fast_factor =
                static_cast<double>(std::get<2>(arguments).value);
        }
        if (std::get<3>(arguments).present) {
            ret.max_delta_per_tick =
                static_cast<double>(std::get<3>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<4>(arguments).present) {
            ret.max_rise_per_tick =
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<double>(std::get<4>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<5>(arguments).present) {
            ret.max_cummulative_rise =
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<double>(std::get<5>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<6>(arguments).present) {
            ret.p_filter_alpha =
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<double>(std::get<6>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<7>(arguments).present) {
            ret.min_window_time =
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<double>(std::get<7>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<8>(arguments).present) {
            ret.max_window_time =
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<double>(std::get<8>(arguments).value);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (std::get<9>(arguments).present) {
            ret.enable_waste_full =
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
                static_cast<bool>(std::get<9>(arguments).value);
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

struct GetWasteDetectionConfig {
    /*
     * M128- GetWasteDetectionConfig get the waste detection configuration
     * */
    using ParseResult = std::optional<GetWasteDetectionConfig>;
    static constexpr auto prefix = std::array{'M', '1', '2', '8'};

    template <typename InputIt, typename InLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputIt, InLimit>
    static auto write_response_into(
        InputIt buf, InLimit limit, bool enabled, double p1, double p2,
        double baseline_factor, double max_delta_per_tick,
        double max_rise_per_tick, double max_cummulative_rise,
        double p_filter_alpha, double min_window_time, double max_window_time)
        -> InputIt {
        int res = 0;
        res = snprintf(&*buf, (limit - buf),
                       "M128 E:%d S:%.1f P:%.1f F:%.1f D:%.1f R:%.1f C:%.1f "
                       "A:%.1f M:%.1f X:%.1f OK\n",
                       enabled, p1, p2, baseline_factor, max_delta_per_tick,
                       max_rise_per_tick, max_cummulative_rise, p_filter_alpha,
                       min_window_time, max_window_time);
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
        return std::make_pair(ParseResult(GetWasteDetectionConfig()), working);
    }
};

}  // namespace gcode
