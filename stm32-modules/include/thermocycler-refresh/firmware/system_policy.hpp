#pragma once

#include <array>

#include "core/xt1511.hpp"
#include "system_hardware.h"
#include "system_serial_number.h"
#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"

class SystemPolicy {
  private:
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    static constexpr uint8_t ADDRESS_LENGTH = 8;
    static constexpr uint8_t ADDRESSES =
        SYSTEM_SERIAL_NUMBER_LENGTH / ADDRESS_LENGTH;
    static constexpr std::size_t LED_BUFFER_SIZE =
        (SYSTEM_LED_COUNT * xt1511::SINGLE_PIXEL_BUF_SIZE) + 1;

  public:
    using LedBuffer = std::array<uint16_t, LED_BUFFER_SIZE>;
    auto enter_bootloader() -> void;
    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number() -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;
    [[nodiscard]] auto get_front_button_status() -> bool;
    // Functions for XT1511 setting
    auto start_send(LedBuffer& buffer) -> bool;
    auto end_send() -> void;
    auto wait_for_interrupt(uint32_t timeout_ms) -> bool;
    [[nodiscard]] auto get_max_pwm() -> uint16_t;
};
