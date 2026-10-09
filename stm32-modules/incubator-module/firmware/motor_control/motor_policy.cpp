#include "firmware/motor_policy.hpp"

#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/motor_hardware.h"
#include "task.h"

using namespace motor_policy;

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::enable_motor(MotorID motor_id) -> bool {
    return hw_enable_motor(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::start_motor_timer(MotorID motor_id) -> void {
    hw_start_motor_timer(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::disable_motor(MotorID motor_id) -> bool {
    return hw_disable_motor(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::stop_motor(MotorID motor_id) -> bool {
    return hw_stop_motor(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::step(MotorID motor_id) -> void { hw_step_motor(motor_id); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::set_direction(MotorID motor_id, bool direction) -> void {
    hw_set_direction(motor_id, direction);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::check_home_sensor(MotorID motor_id) -> bool {
    return hw_read_home_sensor(motor_id);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::check_diag0() -> bool { return hw_read_diag0(); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::set_diag0_irq(bool enable) -> void {
    hw_set_diag0_irq(enable);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::is_diag0_pin(uint16_t pin) -> bool {
    return hw_is_diag0_pin(pin);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::sleep_ms(uint32_t ms) -> void {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
