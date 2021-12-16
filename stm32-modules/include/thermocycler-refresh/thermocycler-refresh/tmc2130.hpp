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
auto pop_bits(uint64_t &val) -> Ret {
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
auto push_bits(uint64_t &val, uint64_t bits) -> void {
    // Masks Len number of bits (1 << 4 is 0b00001)
    static constexpr uint64_t bitmask = (1 << Len) - 1;
    val = val << Len;
    val |= (bits & bitmask);
};

struct GConf {
    static constexpr Registers address = Registers::general_config;
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

    auto set(uint64_t reg) -> void {
        i_scale_analog = pop_bits<1>(reg);
        internal_rsense = pop_bits<1>(reg);
        en_pwm_mode = pop_bits<1>(reg);
        static_cast<void>(pop_bits<1>(reg));
        shaft = pop_bits<1>(reg);
        diag0_error = pop_bits<1>(reg);
        diag0_otpw = pop_bits<1>(reg);
        diag0_stall = pop_bits<1>(reg);
        diag1_stall = pop_bits<1>(reg);
        diag1_index = pop_bits<1>(reg);
        diag1_onstate = pop_bits<1>(reg);
        diag1_steps_skipped = pop_bits<1>(reg);
        diag0_int_pushpull = pop_bits<1>(reg);
        diag1_pushpull = pop_bits<1>(reg);
        small_hysteresis = pop_bits<1>(reg);
        stop_enable = pop_bits<1>(reg);
        direct_mode = pop_bits<1>(reg);
    }

    uint8_t i_scale_analog : 1 = 0;
    uint8_t internal_rsense : 1  = 0;
    uint8_t en_pwm_mode : 1  = 0;
    //uint8_t enc_commutation : 1  = 0; // Disabled, must be 0
    uint8_t shaft : 1  = 0;
    uint8_t diag0_error : 1  = 0;
    uint8_t diag0_otpw : 1  = 0;
    uint8_t diag0_stall : 1  = 0;
    uint8_t diag1_stall : 1  = 0;
    uint8_t diag1_index : 1  = 0;
    uint8_t diag1_onstate : 1  = 0;
    uint8_t diag1_steps_skipped : 1  = 0;
    uint8_t diag0_int_pushpull : 1  = 0;
    uint8_t diag1_pushpull : 1  = 0;
    uint8_t small_hysteresis : 1  = 0;
    uint8_t stop_enable : 1  = 0;
    uint8_t direct_mode : 1  = 0;
    //uint8_t test_mode : 1 = 0; // Disabled, must be 0
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
concept TMC2130Register = requires (Reg& r, uint64_t value) {
    // Struct has a valid register address
    std::same_as<decltype(r.address), Registers&>;
    // Struct has a serialize() function
    { r.serialize() } -> std::same_as<uint64_t>;
    // Struct has a constructor from a serialized value
    { r.set(value) } -> std::same_as<void>;
};

class TMC2130 {
  public:

    template <TMC2130Policy Policy, TMC2130Register Reg>
    auto set_register(Policy& policy, Reg& reg) -> bool {
        return policy.write_register(reg.address, reg.serialize());
    }

    template <TMC2130Policy Policy, TMC2130Register Reg>
    auto get_register(Policy& policy, Reg& reg) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        auto ret = policy.read_register(reg.address);
        if(!ret.has_value()) {
            return RT();
        }
        return RT(reg.set(ret.value()));
    }
};
}
