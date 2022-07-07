#pragma once

#include <array>

#include "heater-shaker/errors.hpp"
#include "systemwide.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#include "system_serial_number.h"
#pragma GCC diagnostic pop

class SystemPolicy {
  private:
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    static constexpr uint8_t ADDRESS_LENGTH = 8;
    static constexpr uint8_t ADDRESSES =
        SYSTEM_SERIAL_NUMBER_LENGTH / ADDRESS_LENGTH;

  public:
    auto enter_bootloader() -> void;
    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number(void)
        -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;
    auto start_set_led(LED_MODE mode, uint8_t pwm_setting) -> errors::ErrorCode;
    auto check_I2C_ready(void) -> bool;
    auto delay_time_ms(uint16_t time_ms) -> void;
};
