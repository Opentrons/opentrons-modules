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

//bool return for error checking?
auto MotorPolicy::lid_stepper_set_vref(uint16_t target_vref_mV) -> void {
    // hardware has a 1ohm sense resistor and the driver has an implicit 10x
    // divider. the dac can express a max of 3.3V, so the maximum current we can
    // drive is 330mA at 3.3V/dac fullscale of 255. we can therefore clamp the
    // current input to 330
    //current_ma =
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

auto MotorPolicy::lid_solenoid_disengage() -> void {
    motor_hardware_solenoid_release();
}

auto MotorPolicy::lid_solenoid_engage() -> void {
    motor_hardware_solenoid_engage();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::delay_ticks(uint16_t ticks) -> void { vTaskDelay(ticks); }
