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
            // Check if we crossed the TRUE threshold temp
            if (crossed_setpoint(true)) {
                _status = PlateStatus::OVERSHOOT;
                _remaining_overshoot_time = OVERSHOOT_TIME;
                _left.temp_target = _current_setpoint;
                _right.temp_target = _current_setpoint;
                _center.temp_target = _current_setpoint;
            } else {
                update_ramp(_left, time);
                update_ramp(_right, time);
                update_ramp(_center, time);
            }
            break;
        case PlateStatus::INITIAL_COOL:
            // Check if we crossed the TRUE threshold temp
            if (crossed_setpoint(false)) {
                _status = PlateStatus::OVERSHOOT;
                _remaining_overshoot_time = OVERSHOOT_TIME;
                _left.temp_target = _current_setpoint;
                _right.temp_target = _current_setpoint;
                _center.temp_target = _current_setpoint;
            } else {
                update_ramp(_left, time);
                update_ramp(_right, time);
                update_ramp(_center, time);
            }
            break;
        case PlateStatus::OVERSHOOT:
            _remaining_overshoot_time -= time;
            if (_remaining_overshoot_time <= 0.0F) {
                _current_setpoint = _setpoint;
                _left.temp_target = _setpoint;
                _right.temp_target = _setpoint;
                _center.temp_target = _setpoint;
                _status = PlateStatus::STEADY_STATE;
                _uniformity_error_timer = UNIFORMITY_CHECK_DELAY;
            }
            break;
        case PlateStatus::STEADY_STATE:
            // Hold time is ONLY updated in steady state!
            _remaining_hold_time = std::max(_remaining_hold_time - time,
                                            static_cast<double>(0.0F));
            _uniformity_error_timer = std::max(_uniformity_error_timer - time,
                                               static_cast<double>(0.0F));
            break;
    }

    values.left_power = update_pid(_left, time);
    values.right_power = update_pid(_right, time);
    values.center_power = update_pid(_center, time);

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

auto PlateControl::set_new_target(double setpoint, double volume_ul,
                                  double hold_time, double ramp_rate) -> bool {
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

    auto distance_to_target = std::abs(setpoint - plate_temp());
    if (distance_to_target > UNDERSHOOT_MIN_DIFFERENCE) {
        if (_status == PlateStatus::INITIAL_HEAT) {
            _current_setpoint = calculate_overshoot(_setpoint, volume_ul);
        } else {
            _current_setpoint = calculate_undershoot(_setpoint, volume_ul);
        }
    } else {
        // If we aren't changing by at least UNDERSHOOT_MIN_DIFFERENCE, just
        // go directly to the setpoint
        _current_setpoint = setpoint;
    }
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

// This function *could* be made const, but that obfuscates the intention,
// which is to update the ramp target of a *member* of the class.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto PlateControl::update_ramp(thermal_general::Peltier &peltier, Seconds time)
    -> void {
    if (_ramp_rate == RAMP_INFINITE) {
        peltier.temp_target = _current_setpoint;
    }
    if (peltier.temp_target < _current_setpoint) {
        peltier.temp_target = std::min(
            peltier.temp_target + (_ramp_rate * time), _current_setpoint);
    } else if (peltier.temp_target > _current_setpoint) {
        peltier.temp_target = std::max(
            peltier.temp_target - (_ramp_rate * time), _current_setpoint);
    }
}

// This function *could* be made static, but that obfuscates the intention,
// which is to update the PID of a *member* of the class.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto PlateControl::update_pid(thermal_general::Peltier &peltier, Seconds time)
    -> double {
    auto current_temp = peltier.current_temp();
    if ((_status == PlateStatus::INITIAL_HEAT ||
        _status == PlateStatus::INITIAL_COOL) &&
        moving_away_from_ambient(current_temp, peltier.temp_target)) {
        if (std::abs(current_temp - peltier.temp_target) >
            proportional_band(peltier.pid)) {
            return (peltier.temp_target > current_temp) ? 1.0 : -1.0;
        }
    }

    return peltier.pid.compute(peltier.temp_target - current_temp,
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
    peltier.pid.reset();

    if (_ramp_rate == RAMP_INFINITE) {
        peltier.temp_target = setpoint();
        if(!moving_away_from_ambient(peltier.current_temp(), peltier.temp_target)) {
            peltier.pid.arm_integrator_reset(peltier.temp_target - peltier.current_temp(), 3);
        }
        
    } else {
        peltier.temp_target = plate_temp();
    }
}

// This function *could* be made const, but that obfuscates the intention,
// which is to reset a *member* of the class.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto PlateControl::reset_control(thermal_general::HeatsinkFan &fan) -> void {
    // The fan always just targets the target temperature w/ an offset
    fan.temp_target = _current_setpoint + FAN_SETPOINT_OFFSET;
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
    return (_status == PlateStatus::STEADY_STATE) &&
           (std::abs(_current_setpoint - plate_temp()) < SETPOINT_THRESHOLD);
}

[[nodiscard]] auto PlateControl::thermistor_drift_check() const -> bool {
    if ((_status != PlateStatus::STEADY_STATE) ||
        (_uniformity_error_timer > 0.0F)) {
        return true;
    }
    auto temperatures = get_peltier_temps();
    double min = temperatures.at(0);
    double max = temperatures.at(0);
    for (auto temperature : temperatures) {
        min = std::min(temperature, min);
        max = std::max(temperature, max);
    }
    return std::abs(max - min) <= THERMISTOR_DRIFT_MAX_C;
}

[[nodiscard]] auto PlateControl::get_peltier_temps() const
    -> std::array<double, PELTIER_COUNT * THERM_PER_PELTIER> {
    return std::array<double, PELTIER_COUNT * THERM_PER_PELTIER>{{
        _left.thermistors.first.temp_c,
        _left.thermistors.second.temp_c,
        _center.thermistors.first.temp_c,
        _center.thermistors.second.temp_c,
        _right.thermistors.first.temp_c,
        _right.thermistors.second.temp_c,
    }};
}

[[nodiscard]] auto PlateControl::crossed_setpoint(bool heating) const -> bool {
    if (heating) {
        return plate_temp() >= _setpoint;
    }
    return plate_temp() <= _setpoint;
}

[[nodiscard]] auto PlateControl::crossed_setpoint(
    const thermal_general::Peltier &channel, bool heating) const -> bool {
    if (heating) {
        return channel.current_temp() >= _setpoint;
    }
    return channel.current_temp() <= _setpoint;
}

[[nodiscard]] auto PlateControl::proportional_band(PID &pid) const -> double {
    if (pid.kp() == 0.0F) {
        return 0.0F;
    }
    return 1.0F / pid.kp();
}

[[nodiscard]] auto PlateControl::moving_away_from_ambient(double current, double target) const -> bool {
    auto target_from_ambient = target - TEMPERATURE_AMBIENT;
    auto current_from_ambient = current - TEMPERATURE_AMBIENT;
    // If the new target crosses ambient, we are moving away
    if((target_from_ambient * current_from_ambient) < 0) {
        return true;
    }
    return std::abs(target_from_ambient) > std::abs(current_from_ambient);
}
