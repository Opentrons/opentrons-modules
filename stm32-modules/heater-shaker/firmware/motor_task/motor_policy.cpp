#include <cstdlib>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "mc_interface.h"
#pragma GCC diagnostic pop

#include "motor_policy.hpp"

MotorPolicy::MotorPolicy(MCI_Handle_t *handle) : motor_handle(handle) {}

auto MotorPolicy::set_rpm(int16_t rpm) -> void {
    int16_t current_speed = MCI_GetAvrgMecSpeedUnit(motor_handle);
    uint16_t ramp_time =
        static_cast<int32_t>(std::abs(rpm - current_speed)) * RAMP_MS_PER_RPM;
    MCI_ExecSpeedRamp(motor_handle, rpm, ramp_time);
    if (MCI_GetSTMState(motor_handle) == IDLE) {
        MCI_StartMotor(motor_handle);
    }
}

auto MotorPolicy::stop() -> void { MCI_StopMotor(motor_handle); }

auto MotorPolicy::get_current_rpm() const -> int16_t {
    return MCI_GetAvrgMecSpeedUnit(motor_handle);
}

auto MotorPolicy::get_target_rpm() const -> int16_t {
    return MCI_GetMecSpeedRefUnit(motor_handle);
}
