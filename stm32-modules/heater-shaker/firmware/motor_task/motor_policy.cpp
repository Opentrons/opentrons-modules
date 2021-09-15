#include <algorithm>
#include <cstdlib>
//#include <array>

#include "FreeRTOS.h"
#include "task.h"
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

MotorPolicy::MotorPolicy(motor_hardware_handles* handles)
    : hw_handles(handles) {}

auto MotorPolicy::homing_solenoid_disengage() -> void {
    motor_hardware_solenoid_release(&hw_handles->dac1);
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
    auto dac_val = static_cast<uint8_t>(
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        (dac_intermediate / MAX_SOLENOID_CURRENT_MA) & 0xff);
    motor_hardware_solenoid_drive(&hw_handles->dac1, dac_val);
}

auto MotorPolicy::set_rpm(int16_t rpm) -> ErrorCode {
    if (rpm == 0) {
        stop();
        return ErrorCode::NO_ERROR;
    }
    if (rpm > MAX_APPLICATION_SPEED_RPM || rpm < MIN_APPLICATION_SPEED_RPM) {
        return ErrorCode::MOTOR_ILLEGAL_SPEED;
    }
    const int16_t current_speed = get_current_rpm();
    const int16_t command_01hz = rpm * _01HZ / _RPM;
    const uint32_t speed_diff = std::abs(rpm - current_speed);
    const uint32_t uncapped_ramp_time_ms = speed_diff / ramp_rate_rpm_per_ms;
    const uint16_t ramp_time_ms = std::max(
        static_cast<uint16_t>(uncapped_ramp_time_ms), static_cast<uint16_t>(1));
    MCI_ExecSpeedRamp(hw_handles->mci[0], -command_01hz, ramp_time_ms);
    if (MCI_GetSTMState(hw_handles->mci[0]) == IDLE) {
        MCI_StartMotor(hw_handles->mci[0]);
    }
    return ErrorCode::NO_ERROR;
}

auto MotorPolicy::stop() -> void { MCI_StopMotor(hw_handles->mci[0]); }

auto MotorPolicy::get_current_rpm() const -> int16_t {
    if (IDLE != MCI_GetSTMState(hw_handles->mci[0])) {
        return -MCI_GetAvrgMecSpeedUnit(hw_handles->mci[0]) * _RPM / _01HZ;
    }
    return 0;
}

auto MotorPolicy::get_target_rpm() const -> int16_t {
    if (IDLE != MCI_GetSTMState(hw_handles->mci[0])) {
        return -MCI_GetMecSpeedRefUnit(hw_handles->mci[0]) * _RPM / _01HZ;
    }
    return 0;
}

auto MotorPolicy::set_ramp_rate(int32_t rpm_per_s) -> ErrorCode {
    if (rpm_per_s > MAX_RAMP_RATE_RPM_PER_S ||
        rpm_per_s < MIN_RAMP_RATE_RPM_PER_S) {
        return ErrorCode::MOTOR_ILLEGAL_RAMP_RATE;
    }
    ramp_rate_rpm_per_ms =
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        std::max(static_cast<double>(rpm_per_s) / 1000.0, 1.0);
    return ErrorCode::NO_ERROR;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::delay_ticks(uint16_t ticks) -> void { vTaskDelay(ticks); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::plate_lock_set_power(float power) -> void {
    motor_hardware_plate_lock_on(&hw_handles->tim3, power);
}

auto MotorPolicy::plate_lock_disable() -> void {
    motor_hardware_plate_lock_off(&hw_handles->tim3);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
/*auto MotorPolicy::get_plate_lock_state(void) -> std::array<char, 14> {
    //what type/format is STATE? Is it int?
    plate_lock_state plate_lock_state_enum = motor_get_plate_lock_state();
    //Convert to array. Just make switch case to hardcode output array?
    std::array<char, 14> plate_lock_state_array = {};
    switch(plate_lock_state_enum) {
        case IDLE_CLOSED:
            plate_lock_state_array = std::array<char, 14>{"IDLE_CLOSED"};
            break;
        case OPENING:
            plate_lock_state_array = std::array<char, 14>{"OPENING"};
            break;
        case IDLE_OPEN:
            plate_lock_state_array = std::array<char, 14>{"IDLE_OPEN"};
            break;
        case CLOSING:
            plate_lock_state_array = std::array<char, 14>{"CLOSING"};
            break;
        case IDLE_UNKNOWN:
            plate_lock_state_array = std::array<char, 14>{"IDLE_UNKNOWN"};
            break;
        case UNKNOWN:
            plate_lock_state_array = std::array<char, 14>{"UNKNOWN"};
            break;
        default:
            plate_lock_state_array = std::array<char, 14>{"UNKNOWN"};
    }
    return plate_lock_state_array;
}*/

auto MotorPolicy::set_pid_constants(double kp, double ki, double kd) -> void {
    // These conversions match those in drive_parameters.h and therefore let you
    // just look at the numeric literals there
    static constexpr const double SPEED_UNIT_CONVERSION_SPC =
        static_cast<double>(SPEED_UNIT) / 10.0;
    PID_SetKD(hw_handles->mct[0]->pPIDSpeed,
              static_cast<int16_t>(kd / SPEED_UNIT_CONVERSION_SPC));
    PID_SetKP(hw_handles->mct[0]->pPIDSpeed,
              static_cast<int16_t>(kp / SPEED_UNIT_CONVERSION_SPC));
    PID_SetKI(hw_handles->mct[0]->pPIDSpeed,
              static_cast<int16_t>(ki / SPEED_UNIT_CONVERSION_SPC));
}
