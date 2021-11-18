#pragma once
#include <array>

#include "heater-shaker/errors.hpp"
#include "systemwide.h"

class TestSystemPolicy {
  private:
    bool entered = false;
    bool serial_number_set = false;
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number = {};
    errors::ErrorCode set_serial_number_return = errors::ErrorCode::NO_ERROR;
    uint16_t last_delay = 0;

  public:
    auto enter_bootloader() -> void;
    auto reset_bootloader_entered() -> void;
    auto bootloader_entered() const -> bool;
    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> new_system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number(void)
        -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;
    auto start_set_led(LED_MODE mode) -> errors::ErrorCode;
    auto check_I2C_ready(void) -> bool;
    auto delay_time_ms(uint16_t time_ms) -> void;
    [[nodiscard]] auto test_get_last_delay() const -> uint16_t;
};
