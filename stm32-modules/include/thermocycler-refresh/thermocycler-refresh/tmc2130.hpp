/**
 * @file tmc2130.hpp
 * @brief Interface to control a TMC2130 IC
 */
#pragma once

#include <bitset>
#include <concepts>
#include <cstdint>
#include <optional>

#include "systemwide.h"

namespace tmc2130 {

// Register mapping

enum class Registers {
    general_config = 0x00,
    general_status = 0x01,
    io_input = 0x04,
    ihold_irun = 0x10,
    tpower_down = 0x11,
    tstep = 0x12,
    tpwmthrs = 0x13,
    tcoolthrs = 0x14,
    thigh = 0x15,
    xdirect = 0x2D,
    vdcmin = 0x33,
};

/**
 * @brief This function removes the \c Len lowest bits from a 64 bit integer
 * and automatically shifts it by \c Len bits rightwards
 *
 * @tparam Len The number of bits to remove
 * @tparam Ret The return type, uint8_t by default
 */
template <size_t Len, typename Ret = uint8_t>
auto pop_bits(uint64_t& val) -> Ret {
    // Masks Len number of bits (e.g. ((1 << 4) - 1) is 0b1111)
    static constexpr Ret bitmask = (1 << Len) - 1;
    auto ret = Ret(val & bitmask);
    val = val >> Len;
    return (Ret)ret;
};

/**
 * @brief This function pushes the \c Len lowest bits of \c bits
 * into the variable \c val and then shifts \c val by \c Len bits
 * leftwards
 *
 * @tparam Len The number of bits to push into \c val
 * @param[inout] val The value to modify
 * @param[in] bits The bits to add to \c val
 */
template <size_t Len>
auto push_bits(uint64_t& val, uint64_t bits) -> void {
    // Masks Len number of bits (1 << 4 is 0b00001)
    static constexpr uint64_t bitmask = (1 << Len) - 1;
    val = val << Len;
    val |= (bits & bitmask);
};

struct GConfig {
    static constexpr Registers address = Registers::general_config;
    static constexpr bool readable = true;
    static constexpr bool writable = true;
    auto serialize() -> uint64_t {
        uint64_t ret = 0;
        push_bits<1>(ret, direct_mode);
        push_bits<1>(ret, stop_enable);
        push_bits<1>(ret, small_hysteresis);
        push_bits<1>(ret, diag1_pushpull);
        push_bits<1>(ret, diag0_int_pushpull);
        push_bits<1>(ret, diag1_steps_skipped);
        push_bits<1>(ret, diag1_onstate);
        push_bits<1>(ret, diag1_index);
        push_bits<1>(ret, diag1_stall);
        push_bits<1>(ret, diag0_stall);
        push_bits<1>(ret, diag0_otpw);
        push_bits<1>(ret, diag0_error);
        push_bits<1>(ret, shaft);
        push_bits<1>(ret, 0);
        push_bits<1>(ret, en_pwm_mode);
        push_bits<1>(ret, internal_rsense);
        push_bits<1>(ret, i_scale_analog);

        return ret;
    }

    static auto create(uint64_t reg) -> GConfig {
        GConfig ret;
        ret.i_scale_analog = pop_bits<1>(reg);
        ret.internal_rsense = pop_bits<1>(reg);
        ret.en_pwm_mode = pop_bits<1>(reg);
        static_cast<void>(pop_bits<1>(reg));
        ret.shaft = pop_bits<1>(reg);
        ret.diag0_error = pop_bits<1>(reg);
        ret.diag0_otpw = pop_bits<1>(reg);
        ret.diag0_stall = pop_bits<1>(reg);
        ret.diag1_stall = pop_bits<1>(reg);
        ret.diag1_index = pop_bits<1>(reg);
        ret.diag1_onstate = pop_bits<1>(reg);
        ret.diag1_steps_skipped = pop_bits<1>(reg);
        ret.diag0_int_pushpull = pop_bits<1>(reg);
        ret.diag1_pushpull = pop_bits<1>(reg);
        ret.small_hysteresis = pop_bits<1>(reg);
        ret.stop_enable = pop_bits<1>(reg);
        ret.direct_mode = pop_bits<1>(reg);

        return ret;
    }

    uint8_t i_scale_analog : 1 = 0;
    uint8_t internal_rsense : 1 = 0;
    uint8_t en_pwm_mode : 1 = 0;
    // uint8_t enc_commutation : 1  = 0; // Disabled, must be 0
    uint8_t shaft : 1 = 0;
    uint8_t diag0_error : 1 = 0;
    uint8_t diag0_otpw : 1 = 0;
    uint8_t diag0_stall : 1 = 0;
    uint8_t diag1_stall : 1 = 0;
    uint8_t diag1_index : 1 = 0;
    uint8_t diag1_onstate : 1 = 0;
    uint8_t diag1_steps_skipped : 1 = 0;
    uint8_t diag0_int_pushpull : 1 = 0;
    uint8_t diag1_pushpull : 1 = 0;
    uint8_t small_hysteresis : 1 = 0;
    uint8_t stop_enable : 1 = 0;
    uint8_t direct_mode : 1 = 0;
    // uint8_t test_mode : 1 = 0; // Disabled, must be 0
};

struct GStatus {
    static constexpr Registers address = Registers::general_status;
    static constexpr bool readable = true;
    static constexpr bool writable = false;

    auto serialize() -> uint64_t {
        uint64_t ret = 0;
        push_bits<1>(ret, undervoltage_error);
        push_bits<1>(ret, driver_error);
        push_bits<1>(ret, reset);

        return ret;
    }

    static auto create(uint64_t reg) -> GStatus {
        GStatus ret;
        ret.reset = pop_bits<1>(reg);
        ret.driver_error = pop_bits<1>(reg);
        ret.undervoltage_error = pop_bits<1>(reg);

        return ret;
    }

    uint8_t reset : 1 = 0;
    uint8_t driver_error : 1 = 0;
    uint8_t undervoltage_error : 1 = 0;
};

struct CurrentControl {
    static constexpr Registers address = Registers::ihold_irun;
    static constexpr bool readable = false;
    static constexpr bool writable = true;

    auto serialize() -> uint64_t {
        uint64_t ret = 0;
        push_bits<4>(ret, hold_current_delay);
        push_bits<3>(ret, 0);
        push_bits<5>(ret, run_current);
        push_bits<3>(ret, 0);
        push_bits<5>(ret, hold_current);

        return ret;
    }

    static auto create(uint64_t reg) -> CurrentControl {
        CurrentControl ret;
        ret.hold_current = pop_bits<5>(reg);
        static_cast<void>(pop_bits<3>(reg));  // 3 bits of padding until IRUN
        ret.run_current = pop_bits<5>(reg);
        static_cast<void>(
            pop_bits<3>(reg));  // 3 bits of padding until IHOLDDELAY
        ret.hold_current_delay = pop_bits<4>(reg);

        return ret;
    }

    uint8_t hold_current : 5 = 0;
    uint8_t run_current : 5 = 0;
    uint8_t hold_current_delay : 4 = 0;
};

struct PowerDownDelay {
    static constexpr Registers address = Registers::tpower_down;
    static constexpr bool readable = false;
    static constexpr bool writable = true;

    static constexpr double max_time = 4.0F;
    static constexpr uint64_t max_val = 0xFF;

    auto serialize() -> uint64_t {
        return (uint64_t)((double)max_val * (time / max_time));
    }
    static auto create(uint64_t reg) -> PowerDownDelay {
        return PowerDownDelay{.time =
                                  ((double)reg / (double)max_val) * max_time};
    }

    double time = 0.0F;
};
/** Hardware abstraction policy for the TMC2130 communication.*/
template <typename Policy>
concept TMC2130Policy = requires(Policy& p, Registers addr, uint64_t value) {
    // A function to write to a register
    { p.write_register(addr, value) } -> std::same_as<bool>;
    // A function to read from a register
    { p.read_register(addr) } -> std::same_as<std::optional<uint64_t>>;
    // TODO a function to commence a movement
};

/** Template concept to constrain what structures encapsulate registers.*/
template <typename Reg>
concept TMC2130Register = requires(Reg& r, uint64_t value) {
    // Struct has a valid register address
    std::same_as<decltype(r.address), Registers&>;
    // Struct has a bool saying whether it can be read
    std::same_as<decltype(r.readable), bool>;
    // Struct has a bool saying whether it can be written
    std::same_as<decltype(r.writable), bool>;
    // Struct has a serialize() function
    { r.serialize() } -> std::same_as<uint64_t>;
    // Struct has a constructor from a serialized value
    { Reg::create(value) } -> std::same_as<Reg>;
};

class TMC2130 {
  public:
    /**
     * @brief Set a register on the TMC2130
     *
     * @tparam Reg The type of register to set
     * @tparam Policy Abstraction class for actual writing
     * @param[in] policy Instance of the abstraction policy to use
     * @param[in] reg The register to write
     * @return True if the register could be written, false otherwise.
     * Attempts to write to an unwirteable register will throw a static
     * assertion.
     */
    template <TMC2130Register Reg, TMC2130Policy Policy>
    auto set_register(Policy& policy, Reg& reg) -> bool {
        static_assert(Reg::writable);
        return policy.write_register(reg.address, reg.serialize());
    }
    /**
     * @brief Read a register on the TMC2130
     *
     * @tparam Reg The type of register to read
     * @tparam Policy Abstraction class for actual writing
     * @param[in] policy Instance of the abstraction policy to use
     * @return The contents of the register, or nothing if the register
     * can't be read.
     */
    template <TMC2130Register Reg, TMC2130Policy Policy>
    auto get_register(Policy& policy) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        static_assert(Reg::readable);
        auto ret = policy.read_register(Reg::address);
        if (!ret.has_value()) {
            return RT();
        }
        return RT(Reg::create(ret.value()));
    }
};
}  // namespace tmc2130
