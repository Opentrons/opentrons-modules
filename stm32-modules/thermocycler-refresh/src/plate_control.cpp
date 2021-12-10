/**
 * @file plate_control.cpp
 * @brief Defines the PlateControl class, which implements control logic for
 * the thermal plate elements on the Thermocycler.
 * @details This class exists to separate the actual feedback control system
 * for the thermal plate from the logical control of it. The class provides
 * functions to set the parameters of a thermal control step, and then it will
 * handle all of the closed-loop control to get to the desired temperature
 * in the way that the command wants.
 */

#include "thermocycler-refresh/plate_control.hpp"

#include "thermocycler-refresh/thermal_general.hpp"

using namespace plate_control;

auto PlateControl::update_control() -> UpdateRet {
    PlateControlVals values = {0.0F};
    switch (_status) {
        case PlateStatus::INITIAL_HEAT:
        case PlateStatus::INITIAL_COOL:
            if (temp_within_setpoint()) {
                _status = PlateStatus::OVERSHOOT;
            } else {
                update_ramp(_left);
                update_ramp(_right);
                update_ramp(_center);
            }
            break;
        case PlateStatus::OVERSHOOT:
            // TODO overshoot control...  update target, track time
            _status = PlateStatus::STEADY_STATE;
            break;
        case PlateStatus::STEADY_STATE:
            if (plate_temp() > _setpoint) {
                _status = PlateStatus::OVERSHOOT;
            }
            break;
    }

    values.left_power = update_pid(_left);
    values.right_power = update_pid(_right);
    values.center_power = update_pid(_center);
    if (!_fan.manual_control) {
        values.fan_power = update_pid(_fan);
    }

    return UpdateRet(values);
}

auto PlateControl::set_new_target(double setpoint, double ramp_rate,
                                  double hold_time) -> bool {
    _ramp_rate = ramp_rate;
    _hold_time = hold_time;
    _setpoint = setpoint;

    if (ramp_rate == RAMP_INFINITE) {
        _left.temp_target = _setpoint;
        _right.temp_target = _setpoint;
        _center.temp_target = _setpoint;
    } else {
        _left.temp_target = plate_temp();
        _right.temp_target = plate_temp();
        _center.temp_target = plate_temp();
    }
    // The fan always just targets the target temperature w/ an offset
    _fan.temp_target = _setpoint + FAN_SETPOINT_OFFSET;

    // For heating vs cooling, go based off of the average plate. Might
    // have to reconsider this, see how it works for small changes.
    _status = (setpoint > plate_temp()) ? PlateStatus::INITIAL_HEAT
                                        : PlateStatus::INITIAL_COOL;
    return true;
}

[[nodiscard]] auto PlateControl::plate_temp() const -> double {
    return (_left.current_temp() + _right.current_temp() +
            _center.current_temp()) /
           PELTIER_COUNT;
}

[[nodiscard]] auto PlateControl::temp_within_setpoint() const -> bool {
    return std::abs(_setpoint - plate_temp()) < SETPOINT_THRESHOLD;
}
