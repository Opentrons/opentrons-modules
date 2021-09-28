#include <array>
#include <iterator>
#include <ranges>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#include "system_serial_number.h"
#pragma GCC diagnostic pop

#include "system_policy.hpp"
#include "heater-shaker/errors.hpp"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::enter_bootloader() -> void {
    system_hardware_enter_bootloader();
}

auto SystemPolicy::set_serial_number(std::array<char, system_serial_number_length> system_serial_number)
    -> errors::ErrorCode {
    // pass in uint64_t to system_set_serial_number
    for (uint8_t page = 0; page < pages; page++) {
        auto input = system_serial_number.begin() + (page * page_length);
        auto limit = input + page_length;
        //auto limit = system_serial_number.end();
        uint64_t to_write = 0;
        for (ssize_t byte_index = sizeof(to_write) - 1;
            input != limit && byte_index >= 0; input++, byte_index--) {
            to_write |= (static_cast<uint64_t>(*input) << (byte_index * 8));
        }

        if (!system_set_serial_number(to_write, page)) {
            return errors::ErrorCode::SYSTEM_SERIAL_NUMBER_HAL_ERROR;
        }
    }
    return errors::ErrorCode::NO_ERROR;
}

auto SystemPolicy::get_serial_number(void) -> std::array<char, system_serial_number_length> {
    std::array<char, system_serial_number_length> serial_number_array = {"EMPTYSN"};
    for (uint8_t page = 0; page < pages; page++) {
        uint64_t written_serial_number = system_get_serial_number(page);
        // int to bytes
        auto output = serial_number_array.begin() + (page * page_length);
        auto limit = output + page_length;
        for (ssize_t iter = sizeof(written_serial_number) - 1;
            iter >= 0 && output != limit; iter--, output++) {
            *output = (written_serial_number >> (iter * 8));
        }
    }
    return serial_number_array;
}
