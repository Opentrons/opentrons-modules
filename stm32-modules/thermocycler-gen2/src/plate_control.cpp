/**
 * @file plate_control.cpp
 * @brief Defines the PlateControl class, which implements control logic for
 * the thermal plate peltiers on the Thermocycler.
 * @details This class exists to separate the actual feedback control system
 * for the thermal plate from the logical control of it. The class provides
 * functions to set the parameters of a thermal control step, and then it will
 * handle all of the closed-loop control to get to the desired temperature
 * in the way that the command wants.
 */

#include "thermocycler-gen2/plate_control.hpp"

#include "thermocycler-gen2/thermal_general.hpp"

using namespace plate_control;

auto PlateControl::update_control(Seconds time) -> UpdateRet {
    PlateControlVals values = {0.0F};
    switch (_status) {
        case PlateStatus::INITIAL_HEAT:
            [[fallthrough]];
        case PlateStatus::INITIAL_COOL:
            if (temp_within_setpoint()) {
                _status = PlateStatus::OVERSHOOT;
            } else {
                update_ramp(_left, time);
                update_ramp(_right, time);
                update_ramp(_center, time);
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

    values.left_power = update_pid(_left, time);
    values.right_power = update_pid(_right, time);
    values.center_power = update_pid(_center, time);

    // Hold time decreases whenever temp has reached the target
    if (_status == PlateStatus::OVERSHOOT ||
        _status == PlateStatus::STEADY_STATE) {
        _remaining_hold_time =
            std::max(_remaining_hold_time - time, static_cast<double>(0.0F));
    }

    // Caller should check whether fan is manual after this function runs
    if (_fan.manual_control) {
        values.fan_power = 0.0F;
        // If we exceed this threshold, force the fan out of manual mode
        if (_fan.current_temp() > IDLE_FAN_INACTIVE_THRESHOLD) {
            _fan.manual_control = false;
        }
    }
    if (!_fan.manual_control) {
        values.fan_power = update_fan(time);
    }

    return UpdateRet(values);
}

auto PlateControl::set_new_target(double setpoint, double hold_time,
                                  double ramp_rate) -> bool {
    _ramp_rate = ramp_rate;
    _hold_time = hold_time;
    _remaining_hold_time = hold_time;
    _setpoint = setpoint;

    reset_control(_left);
    reset_control(_right);
    reset_control(_center);
    reset_control(_fan);

    // For heating vs cooling, go based off of the average plate. Might
    // have to reconsider this, see how it works for small changes.
    _status = (setpoint > plate_temp()) ? PlateStatus::INITIAL_HEAT
                                        : PlateStatus::INITIAL_COOL;
    return true;
}

[[nodiscard]] auto PlateControl::fan_idle_power() const -> double {
    auto temp = _fan.current_temp();
    if (temp < IDLE_FAN_INACTIVE_THRESHOLD) {
        return 0.0F;
    }
    if (temp > IDLE_FAN_DANGER_THRESHOLD) {
        // Force the fan out of manual mode
        _fan.manual_control = false;
        return IDLE_FAN_DANGER_POWER;
    }
    return temp * IDLE_FAN_POWER_SLOPE;
}

[[nodiscard]] auto PlateControl::calculate_overshoot(double setpoint,
                                                     double volume_ul)
    -> double {
    return setpoint + (OVERSHOOT_M_CONST * volume_ul) + OVERSHOOT_B_CONST;
}

[[nodiscard]] auto PlateControl::calculate_undershoot(double setpoint,
                                                      double volume_ul)
    -> double {
    return setpoint + (UNDERSHOOT_M_CONST * volume_ul) + UNDERSHOOT_B_CONST;
}

// This function *could* be made const, but that obfuscates the intention,
// which is to update the ramp target of a *member* of the class.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto PlateControl::update_ramp(thermal_general::Peltier &peltier, Seconds time)
    -> void {
    if (_ramp_rate == RAMP_INFINITE) {
        peltier.temp_target = _setpoint;
    }
    if (peltier.temp_target < _setpoint) {
        peltier.temp_target =
            std::min(peltier.temp_target + (_ramp_rate * time), _setpoint);
    } else if (peltier.temp_target > _setpoint) {
        peltier.temp_target =
            std::max(peltier.temp_target - (_ramp_rate * time), _setpoint);
    }
}

// This function *could* be made static, but that obfuscates the intention,
// which is to update the PID of a *member* of the class.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto PlateControl::update_pid(thermal_general::Peltier &peltier, Seconds time)
    -> double {
    return peltier.pid.compute(peltier.temp_target - peltier.current_temp(),
                               time);
}

auto PlateControl::update_fan(Seconds time) -> double {
    // First check is simple... if heatsink is over 75º we have to
    // crank the fans hard.
    if (_fan.current_temp() > IDLE_FAN_DANGER_THRESHOLD) {
        return IDLE_FAN_DANGER_POWER;
    }
    // Note that all error calculations are the inverse of peltiers. We have
    // to use the current temperature MINUS the target temperature because
    // fans need to drive with a positive magnitude to lower the temperature.
    auto target_zone = temperature_zone(setpoint());
    if (target_zone == TemperatureZone::COLD) {
        if (_status == PlateStatus::INITIAL_COOL) {
            // Ramping down to a cold temp is always 70% drive
            return FAN_POWER_RAMP_COLD;
        }
        // Holding at a cold temp is PID controlling the heatsink to 60ºC
        if (_fan.temp_target != FAN_TARGET_TEMP_COLD) {
            _fan.temp_target = FAN_TARGET_TEMP_COLD;
            _fan.pid.arm_integrator_reset(_fan.current_temp() -
                                          FAN_TARGET_TEMP_COLD);
        }
        // Power is clamped in range [0.35,0.7]
        auto power =
            _fan.pid.compute(_fan.current_temp() - _fan.temp_target, time);
        return std::clamp(power, FAN_POWER_LIMITS_COLD.first,
                          FAN_POWER_LIMITS_COLD.second);
    }
    if (_status == PlateStatus::INITIAL_COOL) {
        // Ramping down to a non-cold temp is always just 55% drive
        return FAN_POWER_RAMP_DOWN_NON_COLD;
    }
    // Ramping up OR holding at a warm/hot temperature means we want to
    // regulate the heatsink to stay under (setpoint - 2)º.
    // There is also a safety threshold of 70º.
    auto threshold = std::min(HEATSINK_SAFETY_THRESHOLD_WARM,
                              setpoint() + FAN_TARGET_DIFF_WARM);
    if (_fan.current_temp() < threshold) {
        return FAN_POWER_UNDER_WARM_THRESHOLD;
    }
    if (_fan.temp_target != threshold) {
        _fan.temp_target = threshold;
        _fan.pid.arm_integrator_reset(_fan.current_temp() - _fan.temp_target);
    }
    auto power = _fan.pid.compute(_fan.current_temp() - _fan.temp_target, time);
    if (target_zone == TemperatureZone::HOT) {
        return std::clamp(power, FAN_POWER_LIMITS_HOT.first,
                          FAN_POWER_LIMITS_HOT.second);
    }
    return std::clamp(power, FAN_POWER_LIMITS_WARM.first,
                      FAN_POWER_LIMITS_WARM.second);
}

// This function *could* be made const, but that obfuscates the intention,
// which is to reset a *member* of the class.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto PlateControl::reset_control(thermal_general::Peltier &peltier) -> void {
    if (_ramp_rate == RAMP_INFINITE) {
        peltier.temp_target = setpoint();
    } else {
        peltier.temp_target = plate_temp();
    }
    peltier.pid.arm_integrator_reset(peltier.temp_target -
                                     peltier.current_temp());
}

// This function *could* be made const, but that obfuscates the intention,
// which is to reset a *member* of the class.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto PlateControl::reset_control(thermal_general::HeatsinkFan &fan) -> void {
    // The fan always just targets the target temperature w/ an offset
    fan.temp_target = _setpoint + FAN_SETPOINT_OFFSET;
    fan.pid.arm_integrator_reset(fan.current_temp() - fan.temp_target);
}

[[nodiscard]] auto PlateControl::plate_temp() const -> double {
    return (_left.current_temp() + _right.current_temp() +
            _center.current_temp()) /
           PELTIER_COUNT;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto PlateControl::temperature_zone(double temp) const
    -> TemperatureZone {
    if (temp < (double)TemperatureZone::COLD) {
        return TemperatureZone::COLD;
    }
    if (temp < (double)TemperatureZone::HOT) {
        return TemperatureZone::WARM;
    }
    return TemperatureZone::HOT;
}

[[nodiscard]] auto PlateControl::get_hold_time() const
    -> std::pair<Seconds, Seconds> {
    return std::make_pair(_remaining_hold_time, _hold_time);
}

[[nodiscard]] auto PlateControl::temp_within_setpoint() const -> bool {
    return std::abs(_setpoint - plate_temp()) < SETPOINT_THRESHOLD;
}
