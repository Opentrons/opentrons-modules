#include "firmware/motor_policy.hpp"

#include <algorithm>
#include <cstdlib>

#include "FreeRTOS.h"
#include "firmware/motor_hardware.h"
#include "task.h"
#include "thermocycler-refresh/errors.hpp"

using namespace errors;

// bool return for error checking?
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_stepper_set_dac(uint8_t dac_val) -> void {
    motor_hardware_lid_stepper_set_dac(dac_val);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_stepper_start(int32_t steps) -> void {
    motor_hardware_lid_stepper_start(steps);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_stepper_stop() -> void {
    motor_hardware_lid_stepper_stop();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_stepper_check_fault() -> bool {
    return motor_hardware_lid_stepper_check_fault();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_stepper_reset() -> bool {
    return motor_hardware_lid_stepper_reset();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_solenoid_disengage() -> void {
    motor_hardware_solenoid_release();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_solenoid_engage() -> void {
    motor_hardware_solenoid_engage();
}
