/**
 * @file is31fl_driver.hpp
 * @brief Implements a driver for the IS31FL LED driver
 *
 */

#pragma once

#include <algorithm>
#include <array>
#include <concepts>

namespace is31fl {

// There are 18 independent channels on the IC
static constexpr size_t IS31FL_CHANNELS = 18;

template <typename Policy>
concept IS31FL_Policy = requires(Policy& policy, uint8_t address, uint8_t reg,
                                 std::array<uint8_t, IS31FL_CHANNELS> array) {
    // Function to write an array of data, starting at a specific
    // register address
    { policy.i2c_write(address, reg, array) } -> std::same_as<bool>;
};

template <uint8_t Address>
class IS31FL {
  public:
    template <IS31FL_Policy Policy>
    auto initialize(Policy& policy) -> bool {
        if (_initialized) {
            return true;
        }

        if (!write_single_reg(SHUTDOWN_REGISTER, SHUTDOWN_VALUE_RUNNING,
                              policy)) {
            return false;
        }

        std::ignore = set_current(0);
        std::ignore = set_pwm(0);

        if (!send_update(policy)) {
            return false;
        }

        _initialized = true;
        return true;
    }

    // Send updated values to the driver
    template <IS31FL_Policy Policy>
    auto send_update(Policy& policy) -> bool {
        if (!policy.i2c_write(Address, PWM_REGISTER_BASE, _pwm_settings)) {
            return false;
        }
        if (!policy.i2c_write(Address, LED_CONTROL_REGISTER_BASE,
                              _current_settings)) {
            return false;
        }
        return write_single_reg(UPDATE_REGISTER, TRIGGER_UPDATE_VALUE, policy);
    }

    // Update current setting for a channel
    auto set_current(size_t channel, float current) -> bool {
        if (channel >= IS31FL_CHANNELS) {
            return false;
        }
        _current_settings.at(channel) = current_reg_conversion(current);
        return true;
    }

    // Update current setting for all channels
    auto set_current(float current) -> bool {
        auto val = current_reg_conversion(current);
        _current_settings.fill(val);
        return true;
    }

    // Update PWM value for a channel
    auto set_pwm(size_t channel, float pwm) -> bool {
        if (channel >= IS31FL_CHANNELS) {
            return false;
        }
        _pwm_settings.at(channel) = pwm_reg_conversion(pwm);
        return true;
    }

    // Update PWM value for all channels
    auto set_pwm(float pwm) -> bool {
        auto val = pwm_reg_conversion(pwm);
        _pwm_settings.fill(val);
        return true;
    }

    auto initialized() -> bool { return _initialized; }

  private:
    template <IS31FL_Policy Policy>
    auto write_single_reg(uint8_t reg, uint8_t value, Policy& policy) -> bool {
        _single_reg_buffer.at(0) = value;
        return policy.i2c_write(Address, reg, _single_reg_buffer);
    }

    /**
     * @brief Helper to convert from a % power to a current register setting
     */
    static auto current_reg_conversion(float power) -> uint8_t {
        power = std::clamp(power, 0.0F, 1.0F);
        return CURRENT_LOOKUP.at(static_cast<size_t>(
            power * static_cast<float>(CURRENT_VALUE_COUNT - 1)));
    }

    /**
     * @brief Helper to convert from a % power to a current register setting
     */
    static auto pwm_reg_conversion(float power) -> uint8_t {
        static constexpr auto max_pwm =
            static_cast<float>(static_cast<uint8_t>(0xFF));
        power = std::clamp(power, 0.0F, 1.0F);
        return static_cast<uint8_t>(power * max_pwm);
    }

    // Shutdown register address
    static constexpr uint8_t SHUTDOWN_REGISTER = 0x00;
    // Shutdown register value to put the chip in "running" mode
    static constexpr uint8_t SHUTDOWN_VALUE_RUNNING = 0x01;

    // Base PWM register. There are 18 channels in a row, starting with
    // this register
    static constexpr uint8_t PWM_REGISTER_BASE = 0x01;

    // Address of the update register
    static constexpr uint8_t UPDATE_REGISTER = 0x13;
    // Set the update register to this in order to trigger an update
    static constexpr uint8_t TRIGGER_UPDATE_VALUE = 0x00;

    // Address of the base LED control register. There are 18 channels
    // in a row, starting with this register
    static constexpr uint8_t LED_CONTROL_REGISTER_BASE = 0x14;

    // Number of unique current settings available
    static constexpr size_t CURRENT_VALUE_COUNT = 14;
    // Current control from lowest to highest
    static constexpr std::array<uint8_t, CURRENT_VALUE_COUNT> CURRENT_LOOKUP = {
        0x00, 0x13, 0x12, 0x11, 0x10, 0x3F, 0x3E,
        0x3A, 0x33, 0x36, 0x32, 0x35, 0x31, 0x30};

    bool _initialized = false;
    std::array<uint8_t, IS31FL_CHANNELS> _current_settings{};
    std::array<uint8_t, IS31FL_CHANNELS> _pwm_settings{};
    std::array<uint8_t, 1> _single_reg_buffer{};
};

};  // namespace is31fl