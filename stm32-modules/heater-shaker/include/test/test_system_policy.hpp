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
    bool test_I2C_ready = false;

  public:
    auto enter_bootloader() -> void;
    auto reset_bootloader_entered() -> void;
    auto bootloader_entered() const -> bool;
    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> new_system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number(void)
        -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;
    auto check_I2C_ready(void) -> bool;
    auto start_set_led(std::array<uint8_t, SYSTEM_WIDE_TXBUFFERSIZE> aTxBuffer)
        -> errors::ErrorCode;
};
