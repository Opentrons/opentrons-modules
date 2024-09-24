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
#include "firmware/motor_driver_policy.hpp"
#include "systemwide.h"
#include "tmc2160_registers.hpp"

namespace tmc2160 {

using namespace motor_driver_policy;
using namespace std::numbers;

class TMC2160 {
  public:
    explicit TMC2160(MotorDriverPolicy* policy)
        : _policy(policy), _initialized(false) {}
    TMC2160(const TMC2160& c) = delete;
    TMC2160(const TMC2160&& c) = delete;
    auto operator=(const TMC2160& c) = delete;
    auto operator=(const TMC2160&& c) = delete;
    ~TMC2160() = default;

    auto initialize(MotorDriverPolicy* policy) -> void {
        _policy = policy;
        _initialized = true;
    }

    auto initialize_config(const TMC2160RegisterMap& registers,
                           MotorID motor_id) -> bool {
        if (!set_register(verify_gconf(registers.gconfig), motor_id)) {
            return false;
        }
        if (!set_register(verify_shortconf(registers.short_conf), motor_id)) {
            return false;
        }
        if (!set_register(verify_drvconf(registers.drvconf), motor_id)) {
            return false;
        }
        if (!set_register(verify_glob_scaler(registers.glob_scale), motor_id)) {
            return false;
        }
        if (!set_register(verify_ihold_irun(registers.ihold_irun), motor_id)) {
            return false;
        }
        if (!set_register(verify_tpowerdown(PowerDownDelay::reg_to_seconds(
                              registers.tpowerdown.time)),
                          motor_id)) {
            return false;
        }
        if (!set_register(registers.tpwmthrs, motor_id)) {
            return false;
        }
        if (!set_register(registers.tcoolthrs, motor_id)) {
            return false;
        }
        if (!set_register(registers.thigh, motor_id)) {
            return false;
        }
        if (!set_register(verify_chopconf(registers.chopconf), motor_id)) {
            return false;
        }
        if (!set_register(verify_coolconf(registers.coolconf), motor_id)) {
            return false;
        }
        if (!set_register(verify_pwmconf(registers.pwmconf), motor_id)) {
            return false;
        }
        return true;
    }

    auto update_current(const TMC2160RegisterMap& registers, MotorID motor_id)
        -> bool {
        return set_register(verify_ihold_irun(registers.ihold_irun), motor_id);
    }

    auto update_chopconf(const TMC2160RegisterMap& registers, MotorID motor_id)
        -> bool {
        return set_register(verify_chopconf(registers.chopconf), motor_id);
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

    /**
     * @brief Build a message to send over SPI
     * @param[in] addr The address to write to
     * @param[in] mode The mode to use, either WRITE or READ
     * @param[in] val The contents to write to the address (0 if this is a read)
     * @return An array with the contents of the message, or nothing if
     * there was an error
     */
    static auto build_message(Registers addr, WriteFlag mode,
                              RegisterSerializedType val)
        -> std::optional<MessageT> {
        using RT = std::optional<MessageT>;
        MessageT buffer = {0};
        auto* iter = buffer.begin();
        auto addr_byte = static_cast<uint8_t>(addr);
        addr_byte |= static_cast<uint8_t>(mode);
        iter = bit_utils::int_to_bytes(addr_byte, iter, buffer.end());
        iter = bit_utils::int_to_bytes(val, iter, buffer.end());
        if (iter != buffer.end()) {
            return RT();
        }
        return RT(buffer);
    }

    auto start_stream(MotorID motor_id) -> void {
        auto buffer = build_message(Registers::DRVSTATUS, WriteFlag::READ, 0);
        if (!buffer.has_value()) {
            return;
        }
        _policy->start_stream(motor_id, buffer.value());
    }

    auto stop_stream() -> void { _policy->stop_stream(); }

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
    template <TMC2160Register Reg>
    requires WritableRegister<Reg>
    auto set_register(Reg reg, MotorID motor_id) -> bool {
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        value &= Reg::value_mask;
        return write(Reg::address, value, motor_id);
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
    template <TMC2160Register Reg>
    requires ReadableRegister<Reg>
    auto read_register(MotorID motor_id) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        auto ret = read(Reg::address, motor_id);
        if (!ret.has_value()) {
            return RT();
        }
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return RT(*reinterpret_cast<Reg*>(&ret.value()));
    }

    /**
     * @brief Write to a register
     *
     * @tparam Policy Type used for bus-level SPI comms
     * @param[in] addr The address to write to
     * @param[in] val The value to write
     * @param[in] policy Instance of \c Policy
     * @return True on success, false on error to write
     */
    auto write(Registers addr, RegisterSerializedType value, MotorID motor_id)
        -> bool {
        auto buffer = build_message(addr, WriteFlag::WRITE, value);
        if (!buffer.has_value()) {
            return false;
        }
        return _policy->tmc2160_transmit_receive(motor_id, buffer.value())
            .has_value();
    }

    /**
     * @brief Read from a register. This actually performs two SPI
     * transactions, as the first one will not return the correct data.
     *
     * @tparam Policy Type used for bus-level SPI comms
     * @param addr The address to read from
     * @param[in] policy Instance of \c Policy
     * @return Nothing on error, read value on success
     */
    auto read(Registers addr, MotorID motor_id)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        auto buffer = build_message(addr, WriteFlag::READ, 0);
        if (!buffer.has_value()) {
            return RT();
        }
        auto ret = _policy->tmc2160_transmit_receive(motor_id, buffer.value());
        if (!ret.has_value()) {
            return RT();
        }
        ret = _policy->tmc2160_transmit_receive(motor_id, buffer.value());
        if (!ret.has_value()) {
            return RT();
        }
        auto* iter = ret.value().begin();
        std::advance(iter, 1);

        RegisterSerializedType retval = 0;
        iter = bit_utils::bytes_to_int(iter, ret.value().end(), retval);
        return RT(retval);
    }

  private:
    MotorDriverPolicy* _policy;
    bool _initialized;
};

}  // namespace tmc2160
