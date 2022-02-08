#include "firmware/motor_policy.hpp"

#include <algorithm>
#include <cstdlib>

#include "FreeRTOS.h"
#include "firmware/motor_hardware.h"
#include "firmware/motor_spi_hardware.h"
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

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_read_closed_switch() -> bool {
    return motor_hardware_lid_read_closed();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_read_open_switch() -> bool {
    return motor_hardware_lid_read_open();
}

auto MotorPolicy::seal_stepper_start(std::function<void()> callback) -> bool {
    _seal_callback = std::move(callback);
    return motor_hardware_start_seal_movement();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::seal_stepper_stop() -> void {
    static_cast<void>(motor_hardware_stop_seal_movement());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::tmc2130_transmit_receive(tmc2130::MessageT& data)
    -> RxTxReturn {
    tmc2130::MessageT retBuf = {0};
    if (motor_spi_sendreceive(data.data(), retBuf.data(), data.size())) {
        return RxTxReturn(retBuf);
    }
    return RxTxReturn();
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::tmc2130_set_enable(bool enable) -> bool {
    return motor_hardware_set_seal_enable(enable);
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::tmc2130_set_direction(bool direction) -> bool {
    return motor_hardware_set_seal_direction(direction);
}
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::tmc2130_step_pulse() -> bool {
    motor_hardware_seal_step_pulse();
    return true;
}
