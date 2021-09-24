#pragma once

#include <array>

#include "heater-shaker/errors.hpp"

class TestSystemPolicy {
  public:
    auto enter_bootloader() -> void;
    auto reset_bootloader_entered() -> void;
    auto bootloader_entered() const -> bool;

    /*auto set_serial_number(std::array<char, system_serial_number_length>
    new_system_serial_number)
        -> errors::ErrorCode;

    auto get_serial_number(void) -> std::array<char,
    system_serial_number_length>;*/

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    auto set_serial_number(std::array<char, 8> new_system_serial_number)
        -> errors::ErrorCode {
        // copy to system_serial_number
        auto copy_start = new_system_serial_number.begin();
        auto copy_length = static_cast<int>(new_system_serial_number.size());
        std::copy(copy_start, (copy_start + copy_length),
                  system_serial_number.begin());
        serial_number_set = true;
        return set_serial_number_return;
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    auto get_serial_number(void) -> std::array<char, 8> {
        if (serial_number_set) {
            return system_serial_number;
        } else {
            std::array<char, 8> empty_serial_number = {"EMPTYSN"};
            return empty_serial_number;
        }
    }

  private:
    bool entered = false;
    bool serial_number_set = false;
    static constexpr std::size_t system_serial_number_length = 8;
    std::array<char, system_serial_number_length> system_serial_number = {};
    errors::ErrorCode set_serial_number_return = errors::ErrorCode::NO_ERROR;
};
