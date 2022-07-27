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

auto TestSystemPolicy::set_passing_color(LED_COLOR color) -> bool {
    passing_color = color;
    return true;
}

auto TestSystemPolicy::start_set_led(LED_COLOR color, uint8_t pwm_setting)
    -> errors::ErrorCode {
    if (color == passing_color) {
        return errors::ErrorCode::NO_ERROR;
    } else {
        return errors::ErrorCode::SYSTEM_LED_TRANSMIT_ERROR;
    }
}

auto TestSystemPolicy::check_I2C_ready(void) -> bool { return true; }

auto TestSystemPolicy::delay_time_ms(uint16_t time_ms) -> void {
    last_delay = time_ms;
}

auto TestSystemPolicy::test_get_last_delay() const -> uint16_t {
    return last_delay;
}
