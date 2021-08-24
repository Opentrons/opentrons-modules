#pragma once

#include <array>
#include <iterator>
#include <ranges>

#include "heater-shaker/errors.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#include "system_serial_number.h"
#pragma GCC diagnostic pop

class SystemPolicy {
  public:
    auto enter_bootloader() -> void;

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    auto set_serial_number(std::array<char, 8> system_serial_number) -> errors::ErrorCode {
        //pass in uint64_t to system_set_serial_number
        auto input = system_serial_number.begin();
        auto limit = system_serial_number.end();
        uint64_t to_write = 0;
        for (ssize_t byte_index = sizeof(to_write) - 1;
            input != limit && byte_index >= 0; input++, byte_index--) {
            to_write |= (static_cast<uint64_t>(*input) << (byte_index * 8));
        }

        if (system_set_serial_number(to_write)) {
            return errors::ErrorCode::NO_ERROR;
        } else {
            return errors::ErrorCode::SYSTEM_SERIAL_NUMBER_HAL_ERROR;
        }
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    auto get_serial_number(void) -> std::array<char, 8> {
        uint64_t written_serial_number = system_get_serial_number();
        //int to bytes
        std::array<char, 8> serial_number_array = {"EMPTYSN"};
        auto output = serial_number_array.begin();
        auto limit = serial_number_array.end();
        for (ssize_t iter = sizeof(written_serial_number) - 1; iter >= 0 && output != limit;
            iter--, output++) {
            *output = (written_serial_number >> (iter * 8));
        }
        return serial_number_array;
    }
};
