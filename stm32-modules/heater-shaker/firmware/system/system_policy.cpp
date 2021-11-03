#include <array>
#include <iterator>
#include <ranges>

#include "FreeRTOS.h"
#include "task.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#include "system_serial_number.h"
#pragma GCC diagnostic pop

#include "heater-shaker/errors.hpp"
#include "system_policy.hpp"

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::enter_bootloader() -> void {
    system_hardware_enter_bootloader();
}

auto SystemPolicy::set_serial_number(
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number)
    -> errors::ErrorCode {
    writable_serial to_write_struct;
    // convert bytes to uint64_t for system_set_serial_number
    // write to 8 chars to each of first 3 addresses on last page of flash
    for (uint8_t address = 0; address < ADDRESSES; address++) {
        auto input = system_serial_number.begin() + (address * ADDRESS_LENGTH);
        auto limit = input + ADDRESS_LENGTH;
        uint64_t to_write = 0;
        for (ssize_t byte_index = sizeof(to_write) - 1;
             input != limit && byte_index >= 0; input++, byte_index--) {
            to_write |= (static_cast<uint64_t>(*input) << (byte_index * 8));
        }
        to_write_struct.contents[address] = to_write;
    }
    if (!system_set_serial_number(&to_write_struct)) {
        return errors::ErrorCode::SYSTEM_SERIAL_NUMBER_HAL_ERROR;
    }
    return errors::ErrorCode::NO_ERROR;
}

auto SystemPolicy::get_serial_number(void)
    -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> {
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> serial_number_array = {
        "EMPTYSN"};
    for (uint8_t address = 0; address < ADDRESSES; address++) {
        uint64_t written_serial_number = system_get_serial_number(address);
        // int to bytes
        auto output = serial_number_array.begin() + (address * ADDRESS_LENGTH);
        auto limit = output + ADDRESS_LENGTH;
        for (ssize_t iter = sizeof(written_serial_number) - 1;
             iter >= 0 && output != limit; iter--, output++) {
            *output = (written_serial_number >> (iter * 8));
        }
    }
    return serial_number_array;
}

auto SystemPolicy::start_set_led_original(std::array<uint8_t, SYSTEM_WIDE_TXBUFFERSIZE> aTxBuffer)
    -> errors::ErrorCode {
    if (!system_hardware_set_led_original(aTxBuffer.data(), LED_Control)) {
        return errors::ErrorCode::SYSTEM_LED_TRANSMIT_START_HAL_ERROR;
    }
    return errors::ErrorCode::NO_ERROR;
}

auto SystemPolicy::start_set_led(void) -> errors::ErrorCode {
    if (!system_hardware_set_led(0)) {
        return errors::ErrorCode::SYSTEM_LED_TRANSMIT_START_HAL_ERROR;
    }
    delay_ticks(100);
    if (!system_hardware_set_led(1)) {
        return errors::ErrorCode::SYSTEM_LED_TRANSMIT_START_HAL_ERROR;
    }
    delay_ticks(100);
    if (!system_hardware_set_led(2)) {
        return errors::ErrorCode::SYSTEM_LED_TRANSMIT_START_HAL_ERROR;
    }
    delay_ticks(100);
    if (!system_hardware_set_led(3)) {
        return errors::ErrorCode::SYSTEM_LED_TRANSMIT_START_HAL_ERROR;
    }
    return errors::ErrorCode::NO_ERROR;
}

auto SystemPolicy::check_I2C_ready(void) -> bool {
    return(system_hardware_I2C_ready());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::delay_ticks(uint16_t ticks) -> void { vTaskDelay(ticks); }
