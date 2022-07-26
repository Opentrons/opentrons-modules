#include "firmware/motor_policy.hpp"

#include <algorithm>
#include <cstdlib>

#include "FreeRTOS.h"
#include "firmware/motor_hardware.h"
#include "firmware/motor_spi_hardware.h"
#include "task.h"
#include "thermocycler-gen2/board_revision.hpp"
#include "thermocycler-gen2/errors.hpp"

using namespace errors;

MotorPolicy::MotorPolicy(bool shared_seal_switch_lines)
    : _seal_callback(),  // NOLINT(readability-redundant-member-init)
      _shared_seal_switch_lines(shared_seal_switch_lines) {}
MotorPolicy::MotorPolicy(MotorPolicy&& other)
    : _seal_callback(std::move(other._seal_callback)),
      _shared_seal_switch_lines(std::move(other._shared_seal_switch_lines)) {}

MotorPolicy::MotorPolicy(const MotorPolicy& other)
    : _seal_callback(other._seal_callback),
      _shared_seal_switch_lines(other._shared_seal_switch_lines) {}

MotorPolicy& MotorPolicy::operator=(MotorPolicy&& other) {
    _seal_callback = std::move(other._seal_callback);
    _shared_seal_switch_lines = std::move(other._shared_seal_switch_lines);
    return *this;
}

MotorPolicy& MotorPolicy::operator=(const MotorPolicy& other) {
    _seal_callback = other._seal_callback;
    _shared_seal_switch_lines = other._shared_seal_switch_lines;
    return *this;
}

// bool return for error checking?
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_stepper_set_dac(uint8_t dac_val) -> void {
    motor_hardware_lid_stepper_set_dac(dac_val);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::lid_stepper_start(int32_t steps, bool overdrive) -> void {
    motor_hardware_lid_stepper_start(steps, overdrive);
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

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::seal_switch_set_extension_armed() -> void {
    motor_hardware_seal_switch_set_extension_armed();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::seal_switch_set_retraction_armed() -> void {
    if (_shared_seal_switch_lines) {
        motor_hardware_seal_switch_set_extension_armed();
    } else {
        motor_hardware_seal_switch_set_retraction_armed();
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::seal_switch_set_disarmed() -> void {
    motor_hardware_seal_switch_set_disarmed();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::seal_read_extension_switch() -> bool {
    return motor_hardware_seal_extension_switch_triggered();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::seal_read_retraction_switch() -> bool {
    if (_shared_seal_switch_lines) {
        return motor_hardware_seal_extension_switch_triggered();
    } else {
        return motor_hardware_seal_retraction_switch_triggered();
    }
}
