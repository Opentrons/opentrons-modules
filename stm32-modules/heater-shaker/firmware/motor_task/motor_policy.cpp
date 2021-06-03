#include <algorithm>
#include <cstdlib>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "drive_parameters.h"
#include "mc_interface.h"
#include "mc_stm_types.h"
#include "motor_hardware.h"
#include "stm32f3xx_hal.h"
#pragma GCC diagnostic pop

#include "heater-shaker/errors.hpp"
#include "motor_policy.hpp"

using namespace errors;

MotorPolicy::MotorPolicy(MCI_Handle_t* handle, DAC_HandleTypeDef* dac1)
    : motor_handle(handle), dac_handle(dac1) {}

auto MotorPolicy::homing_solenoid_disengage() -> void {
    motor_hardware_solenoid_release(dac_handle);
}

auto MotorPolicy::homing_solenoid_engage(uint16_t current_ma) -> void {
    // hardware has a 1ohm sense resistor and the driver has an implicit 10x
    // divider. the dac can express a max of 3.3V, so the maximum current we can
    // drive is 330mA at 3.3V/dac fullscale of 255. we can therefore clamp the
    // current input to 330
    current_ma =
        ((current_ma > MAX_SOLENOID_CURRENT_MA) ? MAX_SOLENOID_CURRENT_MA
                                                : current_ma);
    // and then rescale into 8 bits with 330 ending up at 255
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    auto dac_intermediate = static_cast<uint32_t>(current_ma) * 255;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    auto dac_val = static_cast<uint8_t>(
        (dac_intermediate / MAX_SOLENOID_CURRENT_MA) & 0xff);
    motor_hardware_solenoid_drive(dac_handle, dac_val);
}

auto MotorPolicy::set_rpm(int16_t rpm) -> ErrorCode {
    if (rpm == 0) {
        stop();
        return ErrorCode::NO_ERROR;
    }
    if (rpm > MAX_APPLICATION_SPEED_RPM || rpm < MIN_APPLICATION_SPEED_RPM) {
        return ErrorCode::MOTOR_ILLEGAL_SPEED;
    }
    int16_t current_speed = get_current_rpm();
    int16_t command_01hz = rpm * _01HZ / _RPM;
    uint16_t ramp_time = std::max(
        static_cast<uint16_t>(
            static_cast<int32_t>(std::abs(rpm - current_speed)) / ramp_rate),
        static_cast<uint16_t>(1));
    MCI_ExecSpeedRamp(motor_handle, -command_01hz, ramp_time);
    if (MCI_GetSTMState(motor_handle) == IDLE) {
        MCI_StartMotor(motor_handle);
    }
    return ErrorCode::NO_ERROR;
}

auto MotorPolicy::stop() -> void { MCI_StopMotor(motor_handle); }

auto MotorPolicy::get_current_rpm() const -> int16_t {
    if (IDLE != MCI_GetSTMState(motor_handle)) {
        return -MCI_GetAvrgMecSpeedUnit(motor_handle) * _RPM / _01HZ;
    }
    return 0;
}

auto MotorPolicy::get_target_rpm() const -> int16_t {
    if (IDLE != MCI_GetSTMState(motor_handle)) {
        return -MCI_GetMecSpeedRefUnit(motor_handle) * _RPM / _01HZ;
    }
    return 0;
}

auto MotorPolicy::set_ramp_rate(int32_t rpm_per_s) -> ErrorCode {
    if (rpm_per_s > MAX_RAMP_RATE_RPM_PER_S ||
        rpm_per_s < MIN_RAMP_RATE_RPM_PER_S) {
        return ErrorCode::MOTOR_ILLEGAL_RAMP_RATE;
    }
    ramp_rate = rpm_per_s;
    return ErrorCode::NO_ERROR;
}
