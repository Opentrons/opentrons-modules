#include "firmware/serial.hpp"

#include <iterator>
#include <ranges>

auto Serial::set_serial_number(
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

auto Serial::get_serial_number(void)
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
