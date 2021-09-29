#pragma once
#include <array>

#include "heater-shaker/errors.hpp"
#include "systemwide.hpp"

class TestSystemPolicy {
  private:
    bool entered = false;
    bool serial_number_set = false;
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        systemwide::SERIAL_NUMBER_LENGTH;
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number = {};
    errors::ErrorCode set_serial_number_return = errors::ErrorCode::NO_ERROR;

  public:
    auto enter_bootloader() -> void;
    auto reset_bootloader_entered() -> void;
    auto bootloader_entered() const -> bool;
    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> new_system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number(void)
        -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;
};
