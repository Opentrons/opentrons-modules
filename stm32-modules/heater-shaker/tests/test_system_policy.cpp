#include "test/test_system_policy.hpp"

#include <array>
#include <iterator>

auto TestSystemPolicy::enter_bootloader() -> void { entered = true; }

auto TestSystemPolicy::reset_bootloader_entered() -> void { entered = false; }

auto TestSystemPolicy::bootloader_entered() const -> bool { return entered; }

auto TestSystemPolicy::set_serial_number(
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> new_system_serial_number)
    -> errors::ErrorCode {
    // copy to system_serial_number
    auto copy_start = new_system_serial_number.begin();
    auto copy_length = static_cast<int>(new_system_serial_number.size());
    std::copy(copy_start, (copy_start + copy_length),
              system_serial_number.begin());
    serial_number_set = true;
    return set_serial_number_return;
}

auto TestSystemPolicy::get_serial_number(void)
    -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> {
    if (serial_number_set) {
        return system_serial_number;
    } else {
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> empty_serial_number = {
            "EMPTYSN"};
        return empty_serial_number;
    }
}

auto TestSystemPolicy::check_I2C_ready(void) -> bool {
    if (!test_I2C_ready) {
        return false;
    } else {
        return true;
    }
}

auto TestSystemPolicy::start_set_led(std::array<uint8_t, SYSTEM_WIDE_TXBUFFERSIZE> aTxBuffer)
    -> errors::ErrorCode {
    return errors::ErrorCode::NO_ERROR;
}
