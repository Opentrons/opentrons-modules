#include "firmware/motor_policy.hpp"

#include <algorithm>
#include <cstdlib>

#include "FreeRTOS.h"
#include "firmware/motor_hardware.h"
#include "task.h"
#include "thermocycler-refresh/errors.hpp"

using namespace errors;

// bool return for error checking?
auto MotorPolicy::lid_stepper_set_vref(uint16_t target_vref_mV) -> void {
    // hardware has a 1ohm sense resistor and the driver has an implicit 10x
    // divider. the dac can express a max of 3.3V, so the maximum current we can
    // drive is 330mA at 3.3V/dac fullscale of 255. we can therefore clamp the
    // current input to 330
    // current_ma =
    //    ((current_ma > MAX_SOLENOID_CURRENT_MA) ? MAX_SOLENOID_CURRENT_MA
    //                                            : current_ma);
    // and then rescale into 8 bits with 330 ending up at 255
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    auto dac_intermediate = static_cast<uint32_t>(target_vref_mV) * 255;
    auto dac_val = static_cast<uint8_t>(
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        (dac_intermediate / 330) & 0xff);
    motor_hardware_lid_stepper_set_dac(dac_val);
}

auto MotorPolicy::lid_stepper_start(float angle) -> void {
    motor_hardware_lid_stepper_start(angle);
}

auto MotorPolicy::lid_stepper_stop() -> void {
    motor_hardware_lid_stepper_stop();
}

auto MotorPolicy::lid_stepper_check_fault() -> bool {
    return motor_hardware_lid_stepper_check_fault();
}

auto MotorPolicy::lid_stepper_reset() -> bool {
    return motor_hardware_lid_stepper_reset();
}

auto MotorPolicy::lid_solenoid_disengage() -> void {
    motor_hardware_solenoid_release();
}

auto MotorPolicy::lid_solenoid_engage() -> void {
    motor_hardware_solenoid_engage();
}
