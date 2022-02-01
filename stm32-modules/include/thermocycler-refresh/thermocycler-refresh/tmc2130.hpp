/**
 * @file tmc2130.hpp
 * @brief Interface to control a TMC2130 IC
 */
#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>

#include "core/bit_utils.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/tmc2130_interface.hpp"
#include "thermocycler-refresh/tmc2130_registers.hpp"

namespace tmc2130 {

/** Hardware abstraction policy for the TMC2130 communication.*/
template <typename Policy>
concept TMC2130Policy = TMC2130InterfacePolicy<Policy> && requires(Policy& p) {
    // Configure the TMC to either enabled or not
    { p.tmc2130_set_enable(true) } -> std::same_as<bool>;
    // Function to set direction. true = forward, false = backwards.
    { p.tmc2130_set_direction(true) } -> std::same_as<bool>;
    // Function to pulse the step output
    { p.tmc2130_step_pulse() } -> std::same_as<bool>;
};

class TMC2130 {
  public:
    TMC2130() = delete;
    TMC2130(const TMC2130RegisterMap& registers)
        : _registers(registers), _spi(), _initialized(false) {}

    template <TMC2130Policy Policy>
    auto write_config(Policy& policy) -> bool {
        if (!set_gconf(_registers.gconfig, policy)) {
            return false;
        }
        if (!set_current_control(_registers.ihold_irun, policy)) {
            return false;
        }
        if (!set_power_down_delay(
                PowerDownDelay::reg_to_seconds(_registers.tpowerdown.time),
                policy)) {
            return false;
        }
        if (!set_cool_threshold(_registers.tcoolthrs, policy)) {
            return false;
        }
        if (!set_thigh(_registers.thigh, policy)) {
            return false;
        }
        if (!set_chop_config(_registers.chopconf, policy)) {
            return false;
        }
        if (!set_cool_config(_registers.coolconf, policy)) {
            return false;
        }
        _initialized = true;
        return true;
    }

    template <TMC2130Policy Policy>
    auto write_config(const TMC2130RegisterMap& registers, Policy& policy)
        -> bool {
        if (!set_gconf(registers.gconfig, policy)) {
            return false;
        }
        if (!set_current_control(registers.ihold_irun, policy)) {
            return false;
        }
        if (!set_power_down_delay(
                PowerDownDelay::reg_to_seconds(registers.tpowerdown.time),
                policy)) {
            return false;
        }
        if (!set_cool_threshold(registers.tcoolthrs, policy)) {
            return false;
        }
        if (!set_thigh(registers.thigh, policy)) {
            return false;
        }
        if (!set_chop_config(registers.chopconf, policy)) {
            return false;
        }
        if (!set_cool_config(registers.coolconf, policy)) {
            return false;
        }
        _initialized = true;
        return true;
    }

    /**
     * @brief Check if the TMC2130 has been initialized.
     * @return true if the registers have been written at least once,
     * false otherwise.
     */
    [[nodiscard]] auto initialized() const -> bool { return _initialized; }

    // FUNCTIONS TO SET INDIVIDUAL REGISTERS

    /**
     * @brief Update GCONF register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_gconf(GConfig reg, Policy& policy) -> bool {
        reg.enc_commutation = 0;
        reg.test_mode = 0;
        if (set_register(policy, reg)) {
            _registers.gconfig = reg;
            return true;
        }
        return false;
    }

    /**
     * @brief Update IHOLDIRUN register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_current_control(CurrentControl reg, Policy& policy) -> bool {
        reg.bit_padding_1 = 0;
        reg.bit_padding_2 = 0;
        if (set_register(policy, reg)) {
            _registers.ihold_irun = reg;
            return true;
        }
        return false;
    }
    /**
     * @brief Update TPOWERDOWN register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_power_down_delay(double time, Policy& policy) -> bool {
        PowerDownDelay temp_reg = {.time =
                                       PowerDownDelay::seconds_to_reg(time)};
        if (set_register(policy, temp_reg)) {
            _registers.tpowerdown = temp_reg;
            return true;
        }
        return false;
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
     * @brief Update COOLCONF register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    template <TMC2130Policy Policy>
    auto set_cool_config(CoolConfig reg, Policy& policy) -> bool {
        // Assert that bits that MUST be 0 are actually 0
        reg.padding_1 = 0;
        reg.padding_2 = 0;
        reg.padding_3 = 0;
        reg.padding_4 = 0;
        if (set_register(policy, reg)) {
            _registers.coolconf = reg;
            return true;
        }
        return false;
    }

    /**
     * @brief Get the current GCONF register status. This register can
     * be read, so this function gets it from the actual device.
     */
    template <TMC2130Policy Policy>
    [[nodiscard]] auto get_gconf(Policy& policy) -> std::optional<GConfig> {
        auto ret = read_register<GConfig>(policy);
        if (ret.has_value()) {
            _registers.gconfig = ret.value();
        }
        return ret;
    }
    /**
     * @brief Get the general status register
     */
    template <TMC2130Policy Policy>
    [[nodiscard]] auto get_gstatus(Policy& policy) -> GStatus {
        auto ret = read_register<GStatus>(policy);
        if (ret.has_value()) {
            return ret.value();
        }
        return GStatus{.driver_error = 1};
    }
    /**
     * @brief Get the current CHOPCONF register status. This register can
     * be read, so this function gets it from the actual device.
     */
    template <TMC2130Policy Policy>
    [[nodiscard]] auto get_chop_config(Policy& policy)
        -> std::optional<ChopConfig> {
        auto ret = read_register<ChopConfig>(policy);
        if (ret.has_value()) {
            _registers.chopconf = ret.value();
        }
        return ret;
    }
    /**
     * @brief Get the register map
     */
    [[nodiscard]] auto get_register_map() -> TMC2130RegisterMap& {
        return _registers;
    }

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
    requires WritableRegister<Reg>
    auto set_register(Policy& policy, Reg reg) -> bool {
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        value &= Reg::value_mask;
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
    requires ReadableRegister<Reg>
    auto read_register(Policy& policy) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        auto ret = _spi.read(Reg::address, policy);
        if (!ret.has_value()) {
            return RT();
        }
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return RT(*reinterpret_cast<Reg*>(&ret.value()));
    }

    TMC2130RegisterMap _registers = {};
    TMC2130Interface _spi = {};
    bool _initialized;
};
}  // namespace tmc2130
