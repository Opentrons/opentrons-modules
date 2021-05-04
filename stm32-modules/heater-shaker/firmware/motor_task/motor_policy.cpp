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
    int16_t current_speed = get_current_rpm();
    int16_t command_01hz = rpm * _01HZ / _RPM;
    uint16_t ramp_time =
        static_cast<int32_t>(std::abs(rpm - current_speed)) * RAMP_MS_PER_RPM;
    MCI_ExecSpeedRamp(motor_handle, command_01hz, ramp_time);
    if (MCI_GetSTMState(motor_handle) == IDLE) {
        MCI_StartMotor(motor_handle);
    }
}

auto MotorPolicy::stop() -> void { MCI_StopMotor(motor_handle); }

auto MotorPolicy::get_current_rpm() const -> int16_t {
    return MCI_GetAvrgMecSpeedUnit(motor_handle) * _RPM / _01HZ;
}

auto MotorPolicy::get_target_rpm() const -> int16_t {
    return MCI_GetMecSpeedRefUnit(motor_handle) * _RPM / _01HZ;
}
