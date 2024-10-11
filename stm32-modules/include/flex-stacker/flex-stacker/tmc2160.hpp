/**
 * @file tmc2160.hpp
 * @brief Interface to control a TMC2160 IC
 */
#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <numbers>
#include <optional>

#include "core/bit_utils.hpp"
#include "systemwide.h"
#include "tmc2160_interface.hpp"
#include "tmc2160_registers.hpp"

namespace tmc2160 {

using namespace std::numbers;

class TMC2160 {
  public:
    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto initialize_config(const TMC2160RegisterMap& registers,
                           tmc2160::TMC2160Interface<Policy>& policy,
                           MotorID motor_id) -> bool {
        if (!set_register(verify_gconf(registers.gconfig), policy, motor_id)) {
            return false;
        }
        if (!set_register(verify_shortconf(registers.short_conf), policy,
                          motor_id)) {
            return false;
        }
        if (!set_register(verify_drvconf(registers.drvconf), policy,
                          motor_id)) {
            return false;
        }
        if (!set_register(verify_glob_scaler(registers.glob_scale), policy,
                          motor_id)) {
            return false;
        }
        if (!set_register(verify_ihold_irun(registers.ihold_irun), policy,
                          motor_id)) {
            return false;
        }
        if (!set_register(verify_tpowerdown(PowerDownDelay::reg_to_seconds(
                              registers.tpowerdown.time)),
                          policy, motor_id)) {
            return false;
        }
        if (!set_register(registers.tpwmthrs, policy, motor_id)) {
            return false;
        }
        if (!set_register(registers.tcoolthrs, policy, motor_id)) {
            return false;
        }
        if (!set_register(registers.thigh, policy, motor_id)) {
            return false;
        }
        if (!set_register(verify_chopconf(registers.chopconf), policy,
                          motor_id)) {
            return false;
        }
        if (!set_register(verify_coolconf(registers.coolconf), policy,
                          motor_id)) {
            return false;
        }
        if (!set_register(verify_pwmconf(registers.pwmconf), policy,
                          motor_id)) {
            return false;
        }
        return true;
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto update_current(const TMC2160RegisterMap& registers,
                        tmc2160::TMC2160Interface<Policy>& policy,
                        MotorID motor_id) -> bool {
        return set_register(verify_ihold_irun(registers.ihold_irun), policy,
                            motor_id);
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto update_gconfig(const TMC2160RegisterMap& registers,
                        tmc2160::TMC2160Interface<Policy>& policy,
                        MotorID motor_id) -> bool {
        return set_register(verify_gconf(registers.gconfig), policy, motor_id);
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto update_coolconf(const TMC2160RegisterMap& registers,
                         tmc2160::TMC2160Interface<Policy>& policy,
                         MotorID motor_id) -> bool {
        return set_register(verify_coolconf(registers.coolconf), policy,
                            motor_id);
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto update_chopconf(const TMC2160RegisterMap& registers,
                         tmc2160::TMC2160Interface<Policy>& policy,
                         MotorID motor_id) -> bool {
        return set_register(verify_chopconf(registers.chopconf), policy,
                            motor_id);
    }

    static auto verify_gconf(GConfig reg) -> GConfig {
        reg.test_mode = 0;
        return reg;
    }

    static auto verify_shortconf(ShortConf reg) -> ShortConf {
        reg.bit_padding_1 = 0;
        reg.bit_padding_2 = 0;
        return reg;
    }

    static auto verify_drvconf(DriverConf reg) -> DriverConf {
        reg.bit_padding_1 = 0;
        reg.bit_padding_2 = 0;
        return reg;
    }

    static auto verify_ihold_irun(CurrentControl reg) -> CurrentControl {
        reg.bit_padding_1 = 0;
        reg.bit_padding_2 = 0;
        return reg;
    }

    static auto verify_tpowerdown(double time) -> PowerDownDelay {
        PowerDownDelay temp_reg = {.time =
                                       PowerDownDelay::seconds_to_reg(time)};
        return temp_reg;
    }

    static auto verify_chopconf(ChopConfig reg) -> ChopConfig {
        reg.padding_1 = 0;
        reg.padding_2 = 0;
        return reg;
    }

    static auto verify_coolconf(CoolConfig reg) -> CoolConfig {
        // Assert that bits that MUST be 0 are actually 0
        reg.padding_1 = 0;
        reg.padding_2 = 0;
        reg.padding_3 = 0;
        reg.padding_4 = 0;
        reg.padding_5 = 0;
        return reg;
    }

    static auto verify_pwmconf(StealthChop reg) -> StealthChop {
        reg.padding_0 = 0;
        return reg;
    }

    static auto verify_glob_scaler(GlobalScaler reg) -> GlobalScaler {
        reg.clamp_value();
        return reg;
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] auto verify_sgt_value(std::optional<int> sgt) -> bool {
        if (sgt.has_value()) {
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
            return sgt.value() >= -64 && sgt.value() <= 63;
        }
        return true;
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] auto convert_peak_current_to_tmc2160_value(
        float peak_c, const GlobalScaler& glob_scale,
        const TMC2160MotorCurrentConfig& current_config) const -> uint32_t {
        /*
         * This function allows us to calculate the current scaling value (rms)
         * from the desired peak current in mA to send to the motor driver
         * register
         */
        static constexpr float CS_SCALER = 32.0;
        static constexpr float GLOBAL_SCALER = 256.0;

        auto globale_scaler_inv = 1.0;
        if (glob_scale.global_scaler != 0U) {
            globale_scaler_inv =
                GLOBAL_SCALER / static_cast<float>(glob_scale.global_scaler);
        }
        auto VOLTAGE_INV = current_config.r_sense / current_config.v_sf;
        auto RMS_CURRENT_CONSTANT =
            globale_scaler_inv * CS_SCALER * VOLTAGE_INV;
        auto shifted_current_cs = static_cast<uint64_t>(
            RMS_CURRENT_CONSTANT * peak_c * static_cast<float>(1LL << 32));
        auto current_cs = static_cast<uint32_t>(shifted_current_cs >> 32);
        if (current_cs > 32) {
            current_cs = 32;
        }
        return current_cs - 1;
    }

  private:
    /**
     * @brief Set a register on the TMC2160
     *
     * @tparam Reg The type of register to set
     * @tparam Policy Abstraction class for actual writing
     * @param[in] policy Instance of the abstraction policy to use
     * @param[in] reg The register to write
     * @return True if the register could be written, false otherwise.
     * Attempts to write to an unwirteable register will throw a static
     * assertion.
     */
    template <TMC2160Register Reg, tmc2160::TMC2160InterfacePolicy Policy>
    requires WritableRegister<Reg>
    auto set_register(Reg reg, tmc2160::TMC2160Interface<Policy>& policy,
                      MotorID motor_id) -> bool {
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        value &= Reg::value_mask;
        return policy.write(Reg::address, value, motor_id);
    }
    /**
     * @brief Read a register on the TMC2160
     *
     * @tparam Reg The type of register to read
     * @tparam Policy Abstraction class for actual writing
     * @param[in] policy Instance of the abstraction policy to use
     * @return The contents of the register, or nothing if the register
     * can't be read.
     */
    template <TMC2160Register Reg, tmc2160::TMC2160InterfacePolicy Policy>
    requires ReadableRegister<Reg>
    auto read_register(tmc2160::TMC2160Interface<Policy>& policy,
                       MotorID motor_id) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        auto ret = policy.read(Reg::address, motor_id);
        if (!ret.has_value()) {
            return RT();
        }
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return RT(*reinterpret_cast<Reg*>(&ret.value()));
    }
};

}  // namespace tmc2160
