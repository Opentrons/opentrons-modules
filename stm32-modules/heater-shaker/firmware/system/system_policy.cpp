#include <array>
#include <iterator>
#include <ranges>

#include "FreeRTOS.h"
#include "task.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
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
    return _serial.set_serial_number(system_serial_number);
}

auto SystemPolicy::get_serial_number(void)
    -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> {
    return _serial.get_serial_number();
}

auto SystemPolicy::start_set_led(LED_COLOR color, uint8_t pwm_setting)
    -> errors::ErrorCode {
    if (!system_hardware_set_led(color, pwm_setting)) {
        return errors::ErrorCode::SYSTEM_LED_TRANSMIT_ERROR;
    }
    return errors::ErrorCode::NO_ERROR;
}

auto SystemPolicy::check_I2C_ready(void) -> bool {
    return (system_hardware_I2C_ready());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto SystemPolicy::delay_time_ms(uint16_t time_ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(time_ms));
}
