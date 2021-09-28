#pragma once

#include <array>

#include "heater-shaker/errors.hpp"
#include "systemwide.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#include "system_serial_number.h"
#pragma GCC diagnostic pop

class SystemPolicy {
  private:
    static constexpr std::size_t system_serial_number_length =
        systemwide::serial_number_length;
    static constexpr uint8_t address_length = 8;
    static constexpr uint8_t addresses =
        system_serial_number_length / address_length;

  public:
    auto enter_bootloader() -> void;
    auto set_serial_number(
        std::array<char, system_serial_number_length> system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number(void)
        -> std::array<char, system_serial_number_length>;
};
