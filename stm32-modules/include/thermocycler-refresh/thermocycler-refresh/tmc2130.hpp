/**
 * @file tmc2130.hpp
 * @brief Interface to control a TMC2130 IC
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <optional>

#include "core/bit_utils.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/tmc2130_interface.hpp"
#include "thermocycler-refresh/tmc2130_registers.hpp"

namespace tmc2130 {

/** Hardware abstraction policy for the TMC2130 communication.*/
template <typename Policy>
concept TMC2130Policy = TMC2130InterfacePolicy<Policy>;

class TMC2130 {
  public:
    // FUNCTIONS TO SET INDIVIDUAL REGISTERS

    /**
     * @brief Update GCONF register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_gconf(GConfig reg, Policy& policy) -> bool {
        // Assert that bits that MUST be 0 are actually 0
        if ((reg.enc_commutation != 0) || (reg.test_mode != 0)) {
            return false;
        }
        if (set_register(policy, reg)) {
            _registers.gconfig = reg;
            return true;
        }
        return false;
    }
    /**
     * @brief Get the current GCONF register status. This register can
     * be read, so this function gets it from the actual device.
     */
    template <TMC2130Policy Policy>
    auto get_gconf(Policy& policy) -> GConfig { 
        auto ret = read_register<GConfig>(policy);
        if(ret.has_value()) {
            _registers.gconfig = ret.value();
        }
        return _registers.gconfig;
    }

    /**
     * @brief Update IHOLDIRUN register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_current_control(CurrentControl reg, Policy& policy) -> bool {
        // Assert that bits that MUST be 0 are actually 0
        if ((reg.bit_padding_1 != 0) || (reg.bit_padding_2 != 0)) {
            return false;
        }
        if (set_register(policy, reg)) {
            _registers.ihold_irun = reg;
            return true;
        }
        return false;
    }
    /**
     * @brief Get the current IHOLDIRUN register status
     */
    auto get_current_control() -> CurrentControl {
        return _registers.ihold_irun;
    }

    /**
     * @brief Update TPOWERDOWN register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_power_down_delay(double time, Policy& policy) -> bool {
        PowerDownDelay temp_reg;
        if (temp_reg.set_time(time)) {
            if (set_register(policy, temp_reg)) {
                _registers.tpowerdown = temp_reg;
                return true;
            }
        }
        return false;
    }
    /**
     * @brief Get current power down delay in \b seconds
     */
    auto get_power_down_delay() -> double {
        return _registers.tpowerdown.seconds();
    }

    /**
     * @brief Update TCOOLTHRSH register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_cool_threshold(TCoolThreshold reg, Policy& policy) -> bool {
        if (set_register(policy, reg)) {
            _registers.tcoolthrs = reg;
            return true;
        }
        return false;
    }
    /**
     * @brief Get the current TCOOLTHRSH register status
     */
    auto get_cool_threshold() -> TCoolThreshold { return _registers.tcoolthrs; }

    /**
     * @brief Update THIGH register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_thigh(THigh reg, Policy& policy) -> bool {
        if (set_register(policy, reg)) {
            _registers.thigh = reg;
            return true;
        }
        return false;
    }
    /**
     * @brief Get the current THIGH register status
     */
    auto get_thigh() -> THigh { return _registers.thigh; }

    /**
     * @brief Update CHOPCONF register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_chop_config(ChopConfig reg, Policy& policy) -> bool {
        if (set_register(policy, reg)) {
            _registers.chopconf = reg;
            return true;
        }
        return false;
    }
    /**
     * @brief Get the current CHOPCONF register status
     */
    auto get_chop_config() -> ChopConfig { return _registers.chopconf; }

    /**
     * @brief Update COOLCONF register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_cool_config(CoolConfig reg, Policy& policy) -> bool {
        // Assert that bits that MUST be 0 are actually 0
        if ((reg.padding_1 != 0) || (reg.padding_2 != 0) ||
            (reg.padding_3 != 0) || (reg.padding_4 != 0)) {
            return false;
        }
        if (set_register(policy, reg)) {
            _registers.coolconf = reg;
            return true;
        }
        return false;
    }
    /**
     * @brief Get the current COOLCONF register status
     */
    auto get_cool_config() -> CoolConfig { return _registers.coolconf; }

  private:
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
        auto value = *reinterpret_cast<RegisterSerializedType*>(&reg);
        value &= ((RegisterSerializedType)1 << Reg::bitlen) - 1;
        return _spi.write(Reg::address, value, policy);
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
    auto read_register(Policy& policy) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        static_assert(Reg::readable);
        auto ret = _spi.read(Reg::address, policy);
        if (!ret.has_value()) {
            return RT();
        }
        return RT(*reinterpret_cast<Reg*>(&ret.value()));
    }

    TMC2130RegisterMap _registers = {};
    TMC2130Interface _spi = {};
};
}  // namespace tmc2130
