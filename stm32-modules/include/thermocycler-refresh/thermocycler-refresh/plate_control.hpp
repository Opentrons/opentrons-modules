/**
 * @file plate_control.hpp
 * @brief Defines the PlateControl class, which implements control logic for
 * the thermal plate elements on the Thermocycler.
 *
 */
#pragma once

#include "core/pid.hpp"
#include "thermocycler-refresh/thermal_general.hpp"

namespace plate_control {

/** Enumeration of the control statuses the plate may be in*/
enum class PlateStatus { INITIAL_HEAT, INITIAL_COOL, OVERSHOOT, STEADY_STATE };

enum class TemperatureZone { COLD = 23, WARM = 31, HOT };

struct PlateControlVals {
    double left_power, right_power, center_power, fan_power;
};

// A TemperatureElement is a struct that has a current temperature,
// a goal temperature, and a PID controller.
template <typename E>
concept TemperatureElement = requires(E &e, double err) {
    requires std::same_as<decltype(e.temp_target), double>;
    requires std::same_as<decltype(e.pid), PID>;
    {e.temp_target};
    {e.pid};
    { e.current_temp() } -> std::same_as<double>;
};

class PlateControl {
  public:
    using UpdateRet = std::optional<PlateControlVals>;

    /** This ramp rate value will cause ramped target to immediately become the
     * target.*/
    static constexpr double RAMP_INFINITE = (0.0F);
    /** This hold time means there's no timer for holding.*/
    static constexpr double HOLD_INFINITE = (0.0F);
    static constexpr double PELTIER_COUNT = 3.0F;
    // Max ∆T to be considered "at" the setpoint
    static constexpr double SETPOINT_THRESHOLD = 0.5F;
    // Degrees C *under* the threshold to set the fan
    static constexpr double FAN_SETPOINT_OFFSET = (-2.0F);

    PlateControl() = delete;
    /**
     * @brief Construct a new Plate Control object
     * @param[in] left Left peltier reference
     * @param[in] right Right peltier reference
     * @param[in] center Center peltier reference
     * @param[in] fan Fan control reference
     * @param[in] update_rate Expected number of seconds between each call to
     * \c update_control()
     */
    PlateControl(thermal_general::Peltier &left,
                 thermal_general::Peltier &right,
                 thermal_general::Peltier &center,
                 thermal_general::HeatsinkFan &fan, double update_rate)
        : _left(left),
          _right(right),
          _center(center),
          _fan(fan),
          _update_rate(update_rate) {}

    /**
     * @brief Updates the power settings for the peltiers fans. The current
     * temperature of each Peltier & the Heatsink should be set \b before
     * calling this function, and then the power variables for each element
     * will be updated accordingly.
     * @pre Update the current temperature of each thermistor for each peltier
     * @param setpoint The setpoint to drive the peltiers towards
     * @return A set of updated power outputs, or nothing if an error occurs
     */
    auto update_control() -> UpdateRet;
    /**
     * @brief Set a new target temperature
     *
     * @param setpoint the temperature to drive to
     * @return True if the temperature target could be updated
     */
    auto set_new_target(double setpoint) -> bool {
        return set_new_target(setpoint, RAMP_INFINITE, HOLD_INFINITE);
    }
    /**
     * @brief Set a new target temperature, with configurable ramp rate
     * and hold times
     *
     * @param[in] setpoint The temperature to drive to
     * @param[in] ramp_rate The rate to drive the peltiers at, in degrees
     * celsius per second.
     * @param[in] hold_time The amount of time to hold at the target
     * temperature before the step is considered done, in seconds.
     * @return True if the temperature target could be updated
     */
    auto set_new_target(double setpoint, double ramp_rate, double hold_time)
        -> bool;

    /** Return the current temperature target.*/
    [[nodiscard]] auto setpoint() const -> double { return _setpoint; }
    /** Return the current average temperature of the plate.*/
    [[nodiscard]] auto plate_temp() const -> double;

  private:
    /**
     * @brief Apply a ramp to the target temperature of an element.
     * @param[in] element The element to ramp target temperature of
     */
    template <TemperatureElement Element>
    auto update_ramp(Element &element) -> void {
        if (_ramp_rate == RAMP_INFINITE) {
            element.temp_target = _setpoint;
        }
        if (element.temp_target < _setpoint) {
            element.temp_target =
                std::min(element.temp_target + _ramp_rate, _setpoint);
        } else if (element.temp_target > _setpoint) {
            element.temp_target =
                std::max(element.temp_target - _ramp_rate, _setpoint);
        }
    }
    /**
     * @brief Update the PID control of a single temperature element
     * (fan or peltier).
     * @param[in] element The temperature element (peltier or fan) to update
     * @return The new power value for the element
     */
    template <TemperatureElement Element>
    auto update_pid(Element &element) -> double {
        return element.pid.compute(element.temp_target -
                                   element.current_temp());
    }
    /**
     * @brief Reset a temperature element for a new setpoint
     * @param[in] element The element to reset control for
     */
    template <TemperatureElement Element>
    auto reset_control(Element &element) -> void {
        element.pid.arm_integrator_reset(element.temp_target -
                                         element.current_temp);
    }
    /**
     * @brief Checks if the current plate temperature is within the acceptable
     * bounds for the setpoint
     * @return True if the temperature average is within \ref setpoint_range
     * degrees of the setpoint, false otherwise
     */
    [[nodiscard]] auto temp_within_setpoint() const -> bool;

    PlateStatus _status = PlateStatus::STEADY_STATE;
    thermal_general::Peltier &_left;
    thermal_general::Peltier &_right;
    thermal_general::Peltier &_center;
    thermal_general::HeatsinkFan &_fan;
    // This is the update rate in seconds/tick
    const double _update_rate;

    double _setpoint = 0.0F;
    double _ramp_rate = 0.0F;
    double _hold_time = 0.0F;
};

}  // namespace plate_control
