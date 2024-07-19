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

#include "common/core/bit_utils.hpp"
#include "common/core/logging.h"
#include "tmc2160_registers.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"

namespace tmc2160 {

/** Hardware abstraction policy for the TMC2160 communication.*/
template <typename Policy>
concept TMC2160Policy = TMC2160InterfacePolicy<Policy> && requires(Policy& p) {
    { p.tmc2160_transmit_receive(std::declval<MessageT>()) }
    ->std::same_as<std::optional<MessageT>>;
  };


using namespace std::numbers;

template <class Writer, class TaskQueue>
class TMC2160 {
  public:
    TMC2160() = delete;
    TMC2160(const TMC2160RegisterMap& registers)
        : _registers(registers), _spi(), _initialized(false) {}

    auto read(Registers addr, uint32_t command_data, uint32_t message_index)
        -> void {
        auto converted_addr = static_cast<uint8_t>(addr);
        _spi.read(converted_addr, command_data, _task_queue, _cs_intf,
                          message_index);
    }

    auto write(Registers addr, uint32_t command_data) -> bool {
        auto converted_addr = static_cast<uint8_t>(addr);
        auto response = false;
        // setting a 10 ms timeout and 3 repeats.
        for (int i = 3; i > 0; i--) {
            response = _spi_manager.write(converted_addr, command_data,
                                          _task_queue, _cs_intf, 10);
            if (response) {
                break;
            }
        }
        return response;
    }

    /**
     *
     * @return True if configurations were successfully writen to the spi task
     * queue and false otherwise.
     */
    auto write_config() -> bool {
        if (!set_gconf(_registers.gconfig)) {
            return false;
        }
        if (!set_current_control(_registers.ihold_irun)) {
            return false;
        }
        if (!set_power_down_delay(
                PowerDownDelay::reg_to_seconds(_registers.tpowerdown.time))) {
            return false;
        }
        if (!set_cool_threshold(_registers.tcoolthrs)) {
            return false;
        }
        if (!set_thigh(_registers.thigh)) {
            return false;
        }
        if (!set_chop_config(_registers.chopconf)) {
            return false;
        }
        if (!set_cool_config(_registers.coolconf)) {
            return false;
        }
        if (!set_glob_scaler(_registers.glob_scale)) {
            return false;
        }
        _initialized = true;
        return true;
    }

    /**
     * @brief Check if the TMC2160 has been initialized.
     * @return true if the registers have been written at least once,
     * false otherwise.
     */
    [[nodiscard]] auto initialized() const -> bool { return _initialized; }

    auto handle_spi_read(Registers addr,
                         const spi::utils::MaxMessageBuffer& rxBuffer)
        -> uint32_t {
        uint32_t response = 0;
        const auto* iter = rxBuffer.cbegin();                      // NOLINT
        iter = bit_utils::bytes_to_int(iter + 1, rxBuffer.cend(),  // NOLINT
                                       response);
        switch (addr) {
            case Registers::GCONF:
                update_gconf(response);
                return response;
            case Registers::GSTAT:
                update_gstatus(response);
                return response;
            case Registers::CHOPCONF:
                update_chop_config(response);
                return response;
            case Registers::DRVSTATUS:
                update_driver_status(response);
                return response;
            default:
                return response;
        }
    }

    auto handle_spi_write_failure(Registers addr) -> void {
        // If we fail to set the given register,
        // we should set the value to zero again
        // and treat the driver as uninitialized.
        _initialized = false;
        switch (addr) {
            case Registers::GCONF:
                update_gconf(0);
                break;
            case Registers::GSTAT:
                update_gstatus(0);
                break;
            case Registers::CHOPCONF:
                update_chop_config(0);
                break;
            case Registers::DRVSTATUS:
                update_driver_status(0);
                break;
            default:
                break;
        }
    }

    // FUNCTIONS TO SET INDIVIDUAL REGISTERS

    /**
     * @brief Update GCONF register
     * @param reg New configuration register to set
     * @param policy Instance of abstraction policy to use
     * @return True if new register was set succesfully, false otherwise
     */
    auto set_gconf(GConfig reg) -> bool {
        reg.test_mode = 0;
        if (set_register(reg)) {
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
    auto set_current_control(CurrentControl reg) -> bool {
        reg.bit_padding_1 = 0;
        reg.bit_padding_2 = 0;
        if (set_register(reg)) {
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
    auto set_power_down_delay(double time) -> bool {
        PowerDownDelay temp_reg = {.time =
                                       PowerDownDelay::seconds_to_reg(time)};
        if (set_register(temp_reg)) {
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
    auto set_cool_threshold(TCoolThreshold reg) -> bool {
        if (set_register(reg)) {
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
    auto set_thigh(THigh reg) -> bool {
        if (set_register(reg)) {
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
    auto set_chop_config(ChopConfig reg) -> bool {
        if (set_register(reg)) {
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
    auto set_cool_config(CoolConfig reg) -> bool {
        // Assert that bits that MUST be 0 are actually 0
        reg.padding_1 = 0;
        reg.padding_2 = 0;
        reg.padding_3 = 0;
        reg.padding_4 = 0;
        if (set_register(reg)) {
            _registers.coolconf = reg;
            return true;
        }
        return false;
    }

    /**
     * @brief Update GlobalScaler register
     * @param reg New configuration register to set
     * @return True if new register was set succesfully, false otherwise
     */
    auto set_glob_scaler(GlobalScaler reg) -> bool {
        reg.clamp_value();
        if (set_register(reg)) {
            _registers.glob_scale = reg;
            return true;
        }
        return false;
    }

    [[nodiscard]] auto get_glob_scaler() -> std::optional<GlobalScaler> {
        return _registers.glob_scale;
    }

    /**
     * @brief Get the current GlobalScaler register value. This register can
     * be read, so this function gets it from the actual device.
     */
    auto update_glob_scaler(uint32_t data) -> void {
        auto ret = read_register<GlobalScaler>(data);
        if (ret.has_value()) {
            _registers.glob_scale = ret.value();
        }
    }

    /**
     * @brief Get the current GCONF register status. This register can
     * be read, so this function gets it from the actual device.
     */
    [[nodiscard]] auto get_gconf() -> std::optional<GConfig> {
        return _registers.gconfig;
    }

    auto update_gconf(uint32_t data) -> void {
        auto ret = read_register<GConfig>(data);
        if (ret.has_value()) {
            _registers.gconfig = ret.value();
        }
    }

    /**
     * @brief Get the general status register
     */
    [[nodiscard]] auto get_gstatus() -> GStatus {
        if (bool(_registers.gstat)) {
            return _registers.gstat;
        }
        return GStatus{.driver_error = 1};
    }

    /**
     * @brief Update the general status register
     */
    auto update_gstatus(uint32_t data) -> void {
        auto ret = read_register<GStatus>(data);
        if (ret.has_value()) {
            _registers.gstat = ret.value();
        }
    }
    /**
     * @brief Get the current CHOPCONF register status. This register can
     * be read, so this function gets it from the actual device.
     */
    [[nodiscard]] auto get_chop_config() -> std::optional<ChopConfig> {
        return _registers.chopconf;
    }

    /**
     * @brief Update the current CHOPCONF register status.
     */
    auto update_chop_config(uint32_t data) -> void {
        auto ret = read_register<ChopConfig>(data);
        if (ret.has_value()) {
            _registers.chopconf = ret.value();
        }
    }

    /**
     * @brief Get the current DRV_STATUS register reading. Contains
     * information on the current error & stallguard status of the IC.
     * @return The register, or nothing if the register couldn't be read.
     */
    [[nodiscard]] auto get_driver_status() -> std::optional<DriveStatus> {
        return _registers.drvstatus;
    }

    /**
     * @brief Get the current DRV_STATUS register reading. Contains
     * information on the current error & stallguard status of the IC.
     * @return The register, or nothing if the register couldn't be read.
     */
    auto update_driver_status(uint32_t data) -> void {
        auto ret = read_register<DriveStatus>(data);
        if (ret.has_value()) {
            _registers.drvstatus = ret.value();
        }
    }

    /**
     * @brief Get the register map
     */
    [[nodiscard]] auto get_register_map() -> TMC2160RegisterMap& {
        return _registers;
    }

    [[nodiscard]] auto convert_to_tmc2160_current_value(uint32_t c) const
        -> uint32_t {
        /*
         *
         * The default global scaler constant should be 167.0 or 0xA7. When
         * the run current CS register is set to 31, that will give us a rms run
         * current of approximately 1.5A.
         * From the datasheet (page 62):
         *
         * For best precision of current setting, it is advised to measure and
         * fine tune the current in the application. Choose the sense resistors
         * to the next value covering the desired motor current. Set IRUN to 31
         * corresponding 100% of the desired motor current and fine-tune motor
         * current using GLOBALSCALER. IHOLD should be set to a nominal value
         * of 16.
         *
         * CURRENT_SCALE_RATIO = run_current_scale + 1 / 32 (should always be 1)
         * RMS_CURRENT_RATIO = full scale voltage / resistence
         * GLOB_FROM_CURRENT = 256.0 * sqrt(2)
         * GLOBALSCALAR_CONSTANT = GLOB_FROM_CURRENT / (CURRENT_SCALE_RATIO *
         * RMS_CURRENT_RATIO)
         *
         * new_scalar = current * GLOBALSCALAR_CONSTANT
         *
         * GLOBALSCALAR_CONSTANT = 167
         */
        auto GLOBALSCALAR_INV = 256.0 / _registers.glob_scale.global_scaler;
        auto VOLTAGE_INV = _current_config.r_sense / _current_config.v_sf;
        auto RMS_CURRENT_CONSTANT =
            GLOBALSCALAR_INV * sqrt2 * 32.0 * VOLTAGE_INV;
        LOG("The current rms constant is %.4f", RMS_CURRENT_CONSTANT);
        auto fixed_point_constant = static_cast<uint32_t>(
            RMS_CURRENT_CONSTANT * static_cast<float>(1LL << 16));
        uint64_t shifted_current_cs =
            static_cast<uint64_t>(fixed_point_constant) *
            static_cast<uint64_t>(c);
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
    template <TMC2160Register Reg>
    requires WritableRegister<Reg>
    auto set_register(Reg& reg) -> bool {
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        value &= Reg::value_mask;
        return write(Reg::address, value);
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
    auto read_register(uint32_t data) -> std::optional<Reg> {
        using RT = std::optional<RegisterSerializedType>;
        using RG = std::optional<Reg>;

        auto ret = RT(data);
        if (!ret.has_value()) {
            return RG();
        }
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return RG(*reinterpret_cast<Reg*>(&ret.value()));
    }

    TMC2160RegisterMap _registers = {};
    TMC2160Interface _spi = {};
    bool _initialized;
};

}  // namespace tmc2160

