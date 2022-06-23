#include "firmware/system_policy.hpp"

#include <array>
#include <iterator>
#include <ranges>

#include "firmware/system_hardware.h"
#include "firmware/system_led_hardware.h"
#include "system_serial_number.h"
#include "thermocycler-gen2/errors.hpp"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::enter_bootloader() -> void {
    system_hardware_enter_bootloader();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::set_serial_number(
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number)
    -> errors::ErrorCode {
    writable_serial to_write_struct = {0};
    // convert bytes to uint64_t for system_set_serial_number
    // write to 8 chars to each of first 3 addresses on last page of flash
    for (uint8_t address = 0; address < ADDRESSES; address++) {
        auto *input =
            std::next(system_serial_number.begin(), address * ADDRESS_LENGTH);
        auto *limit = std::next(input, ADDRESS_LENGTH);
        uint64_t to_write = 0;
        for (ssize_t byte_index = sizeof(to_write) - 1;
             input != limit && byte_index >= 0;
             std::advance(input, 1), byte_index--) {
            to_write |= (static_cast<uint64_t>(*input) << (byte_index * 8));
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        to_write_struct.contents[address] = to_write;
    }
    if (!system_set_serial_number(&to_write_struct)) {
        return errors::ErrorCode::SYSTEM_SERIAL_NUMBER_HAL_ERROR;
    }
    return errors::ErrorCode::NO_ERROR;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::get_serial_number()
    -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> {
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> serial_number_array = {
        "EMPTYSN"};
    for (uint8_t address = 0; address < ADDRESSES; address++) {
        uint64_t written_serial_number = system_get_serial_number(address);
        // int to bytes
        auto *output =
            std::next(serial_number_array.begin(), address * ADDRESS_LENGTH);
        auto *limit = std::next(output, ADDRESS_LENGTH);
        for (ssize_t iter = sizeof(written_serial_number) - 1;
             iter >= 0 && output != limit; iter--, std::advance(output, 1)) {
            *output = (written_serial_number >> (iter * 8));
        }
    }
    return serial_number_array;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto SystemPolicy::get_front_button_status() -> bool {
    return system_front_button_pressed();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::start_send(LedBuffer &buffer) -> bool {
    return system_led_start_send(buffer.data(), buffer.size());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::end_send() -> void { system_led_stop(); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::wait_for_interrupt(uint32_t timeout_ms) -> bool {
    return system_led_wait_for_interrupt(timeout_ms);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto SystemPolicy::get_max_pwm() -> uint16_t {
    return system_led_max_pwm();
}
