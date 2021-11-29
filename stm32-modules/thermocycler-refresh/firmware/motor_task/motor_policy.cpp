#include <algorithm>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "motor_hardware.h"
#include "stm32g4xx_hal.h"
#pragma GCC diagnostic pop

#include "thermocycler-refresh/errors.hpp"
#include "motor_policy.hpp"

using namespace errors;

MotorPolicy::MotorPolicy(motor_hardware_handles* handles)
    : hw_handles(handles) {}

auto MotorPolicy::lid_solenoid_disengage() -> void {
    motor_hardware_solenoid_release();
}

auto MotorPolicy::lid_solenoid_engage() -> void {
    motor_hardware_solenoid_engage();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::delay_ticks(uint16_t ticks) -> void { vTaskDelay(ticks); }
