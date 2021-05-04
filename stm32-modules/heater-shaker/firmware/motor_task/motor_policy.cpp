#include <algorithm>
#include <cstdlib>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "mc_interface.h"
#include "mc_stm_types.h"
#pragma GCC diagnostic pop

#include "motor_policy.hpp"

MotorPolicy::MotorPolicy(MCI_Handle_t *handle) : motor_handle(handle) {}

auto MotorPolicy::set_rpm(int16_t rpm) -> void {
    if (rpm == 0) {
        stop();
        return;
    }
    int16_t current_speed = get_current_rpm();
    int16_t command_01hz = rpm * _01HZ / _RPM;
    uint16_t ramp_time = std::max(
        static_cast<uint16_t>(
            static_cast<int32_t>(std::abs(rpm - current_speed)) / ramp_rate),
        static_cast<uint16_t>(1));
    MCI_ExecSpeedRamp(motor_handle, command_01hz, ramp_time);
    if (MCI_GetSTMState(motor_handle) == IDLE) {
        MCI_StartMotor(motor_handle);
    }
}

auto MotorPolicy::stop() -> void { MCI_StopMotor(motor_handle); }

auto MotorPolicy::get_current_rpm() const -> int16_t {
    if (IDLE != MCI_GetSTMState(motor_handle)) {
        return MCI_GetAvrgMecSpeedUnit(motor_handle) * _RPM / _01HZ;
    }
    return 0;
}

auto MotorPolicy::get_target_rpm() const -> int16_t {
    if (IDLE != MCI_GetSTMState(motor_handle)) {
        return MCI_GetMecSpeedRefUnit(motor_handle) * _RPM / _01HZ;
    }
    return 0;
}

auto MotorPolicy::set_ramp_rate(int32_t rpm_per_s) -> void {
    ramp_rate = rpm_per_s;
}
