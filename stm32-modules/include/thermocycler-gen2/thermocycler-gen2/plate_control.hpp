/**
 * @file plate_control.hpp
 * @brief Defines the PlateControl class, which implements control logic for
 * the thermal plate elements on the Thermocycler.
 *
 */
#pragma once

#include "core/pid.hpp"
#include "thermocycler-gen2/thermal_general.hpp"
#include <optional>
#include <array>

namespace plate_control {

/** Enumeration of the control statuses the plate may be in*/
enum class PlateStatus { INITIAL_HEAT, INITIAL_COOL, OVERSHOOT, STEADY_STATE };

enum class TemperatureZone { COLD = 23, WARM = 31, HOT };

struct PlateControlVals {
    double left_power, right_power, center_power, fan_power;
};

class PlateControl {
  public:
    using UpdateRet = std::optional<PlateControlVals>;
    using Seconds = double;  // In seconds

    /** This ramp rate value will cause ramped target to immediately become the
     * target.*/
    static constexpr double RAMP_INFINITE = (0.0F);
    /** This hold time means there's no timer for holding.*/
    static constexpr double HOLD_INFINITE = (0.0F);
    /** Number of peltiers on system.*/
    static constexpr size_t PELTIER_COUNT = 3;
    /** Number of thermistors per peltier.*/
    static constexpr size_t THERM_PER_PELTIER = 2;
    /** Max ∆T to be considered "at" the setpoint.*/
    static constexpr double SETPOINT_THRESHOLD = 2.0F;

    /** Degrees C *under* the threshold to set the fan.*/
    static constexpr double FAN_SETPOINT_OFFSET = (-2.0F);
    /** Below this temperature, an idle fan should be off.*/
    static constexpr double IDLE_FAN_INACTIVE_THRESHOLD = 68.0F;
    /** Above this temperature, an idle fan should be set to 80%.*/
    static constexpr double IDLE_FAN_DANGER_THRESHOLD = 75.0F;
    /** When fan is between INACTIVE_THRESHOLD and DANGER_THRESHOLD, multiply
     * temperature by this constant to set the power*/
    static constexpr double IDLE_FAN_POWER_SLOPE = (1.0F / 100.0F);
    /** Power to set a fan when temp exceeds DANGER_THRESHOLD.*/
    static constexpr double IDLE_FAN_DANGER_POWER = (0.8F);
    /** Power to set when ramping down to cold temperature.*/
    static constexpr double FAN_POWER_RAMP_COLD = 0.7F;
    /** Temperature to hold on heatsink when holding at cold temperature.*/
    static constexpr double FAN_TARGET_TEMP_COLD = 60.0F;
    /** Min & max power settings when holding at a cold temp.*/
    static constexpr std::pair<double, double> FAN_POWER_LIMITS_COLD{0.35, 0.7};
    /** Fan power when ramping down to a non-cold temperature.*/
    static constexpr double FAN_POWER_RAMP_DOWN_NON_COLD = 0.55;
    /** Safety threshold of heatsink at warm/hot temperature.*/
    static constexpr double HEATSINK_SAFETY_THRESHOLD_WARM = 70.0F;
    /** Fan power when under safety threshold in warm/hot zone.*/
    static constexpr double FAN_POWER_UNDER_WARM_THRESHOLD = 0.15F;
    /** When controlling to a warm temperature, scale fans to try to keep the
     *  heatsink at (setpoint - 2º).*/
    static constexpr double FAN_TARGET_DIFF_WARM = -2.0F;
    /** Min & max power settings when holding at a warm temp.*/
    static constexpr std::pair<double, double> FAN_POWER_LIMITS_WARM{0.35,
                                                                     0.55};
    /** Min & max power settings when holding at a hot temp.*/
    static constexpr std::pair<double, double> FAN_POWER_LIMITS_HOT{0.30, 0.55};
    /** Slope for overshoot & undershoot, in C/µL */
    static constexpr double OVERSHOOT_DEGREES_PER_MICROLITER = (2.0F / 50.0F);
    /** Minimum volume to trigger overshoot/undershoot */
    static constexpr double OVERSHOOT_MIN_VOLUME_MICROLITERS = 20.0F;
    /** Slope for overshoot & undershoot, in C/µL */
    static constexpr double UNDERSHOOT_DEGREES_PER_MICROLITER =
        -OVERSHOOT_DEGREES_PER_MICROLITER;
    /** Minimum volume to trigger overshoot/undershoot */
    static constexpr double UNDERSHOOT_MIN_VOLUME_MICROLITERS =
        OVERSHOOT_MIN_VOLUME_MICROLITERS;
    /** Minimum temperature difference to trigger overshoot, in ºC.*/
    static constexpr double UNDERSHOOT_MIN_DIFFERENCE = 5.0F;
    /** Margin where controller switches targets from overshoot/undershoot
     * to actual target */
    static constexpr double OVERSHOOT_TARGET_SWITCH_DIFFERENCE = 1.0F;
    /** Maximum drift between thermistors at steady state, in ºC.*/
    static constexpr double THERMISTOR_DRIFT_MAX_C = 4.0F;
    /** Minimum time that the system must be in steady state before
     *  checking for uniformity errors.*/
    static constexpr Seconds UNIFORMITY_CHECK_DELAY = 30.0F;
    /** Approximation of ambient temperature */
    static constexpr double TEMPERATURE_AMBIENT = 23.0F;
    /** How far from target temp to reset Integral Windup.*/
    static constexpr double WINDUP_RESET_THRESHOLD = 3.0F;
    /** Maximum time in seconds for overshoot to apply.*/
    static constexpr double MAX_HOLD_TIME_FOR_OVERSHOOT = 120.0F;
    /** When HEATING to a target below ambient temperature, adjust
     *  the initial overshoot/undershoot target by this amount to
     *  reduce the effects of over-overshooting.*/
    static constexpr double TARGET_ADJUST_FOR_COLD_TARGET = -5.0F;
    /** Extra factor to multiply the proportioal band by */
    static constexpr double PROPORTIONAL_BAND_EXTRA_FACTOR = 2.0F;
    /**
     * When performing the thermistor drift check, this is the max point
     * below which errors are ignored. This is added to prevent unnecesary
     * error messages during long periods below 8º where temperature may
     * drift more than our normal spec BUT will not affect the samples.
     */
    static constexpr double DRIFT_CHECK_IGNORE_MAX_TEMP = 7.5;

    PlateControl() = delete;
    /**
     * @brief Construct a new Plate Control object
     * @param[in] left Left peltier reference
     * @param[in] right Right peltier reference
     * @param[in] center Center peltier reference
     * @param[in] fan Fan control reference
     */
    PlateControl(thermal_general::Peltier &left,
                 thermal_general::Peltier &right,
                 thermal_general::Peltier &center,
                 thermal_general::HeatsinkFan &fan)
        : _left(left), _right(right), _center(center), _fan(fan) {}

    /**
     * @brief Updates the power settings for the peltiers fans. The current
     * temperature of each Peltier & the Heatsink should be set \b before
     * calling this function, and then the power variables for each element
     * will be updated accordingly.
     * @note After executing this function, check the status of the
     * \c manual_control variable in the fan handle. This function will set the
     * flag to \c false if the fan temperature exceeds a safety thershold.
     * @pre Update the current temperature of each thermistor for each peltier
     * @param time The amount of time that has elapsed since the function was
     * last called, in seconds
     * @return A set of updated power outputs, or nothing if an error occurs
     */
    auto update_control(Seconds time) -> UpdateRet;
    /**
     * @brief Set a new target temperature, with configurable ramp rate
     * and hold times
     *
     * @param[in] setpoint The temperature to drive to
     * @param[in] hold_time The amount of time to hold at the target
     * temperature before the step is considered done, in seconds.
     * @param[in] ramp_rate The rate to drive the peltiers at, in degrees
     * celsius per second.
     * @return True if the temperature target could be updated
     */
    auto set_new_target(double setpoint, double volume_ul,
                        double hold_time = HOLD_INFINITE,
                        double ramp_rate = RAMP_INFINITE) -> bool;

    /**
     * @brief This function will return the correct fan PWM to be set if
     * the fan is in idle mode, as a percentage from 0 to 1.0.
     * @note After executing this function, check the status of the
     * \c manual_control variable in the fan handle. This function will set the
     * flag to \c false if the fan temperature exceeds a safety thershold.
     * @return The fan power that should be set, based off of the current
     * temperature of the heatsink.
     */
    [[nodiscard]] auto fan_idle_power() const -> double;

    /** Return the current temperature target.*/
    [[nodiscard]] auto setpoint() const -> double { return _setpoint; }

    /** Return the current average temperature of the plate.*/
    [[nodiscard]] auto plate_temp() const -> double;

    /** Return the current PlateStatus.*/
    [[nodiscard]] auto status() const -> PlateStatus { return _status; }

    /** Get the TemperatureZone that a temperature falls into.*/
    [[nodiscard]] auto temperature_zone(double temp) const -> TemperatureZone;

    /**
     * @brief Return the remaining and total hold times, in seconds.
     * @return Pair of <remaining hold time, total hold time>
     */
    [[nodiscard]] auto get_hold_time() const -> std::pair<Seconds, Seconds>;

    /**
     * @brief Checks if the current plate temperature is within the acceptable
     * bounds for the setpoint
     * @return True if the temperature average is within \ref setpoint_range
     * degrees of the setpoint, false otherwise
     */
    [[nodiscard]] auto temp_within_setpoint() const -> bool;

    /**
     * @brief Check for thermistor drift.
     * @return true if the thermistors are \b within spec, false if the
     *         drift between any two thermistors is over 4ºC
     */
    [[nodiscard]] auto thermistor_drift_check() const -> bool;

    /**
     * @brief Get the temperature of every peltier
     *
     * @return std::array containing each peltier thermistor temperature.
     */
    [[nodiscard]] auto get_peltier_temps() const
        -> std::array<double, PELTIER_COUNT * THERM_PER_PELTIER>;

    /**
     * @brief Calculate the overshoot target temperature based off of a
     * setpoint temperature and a liquid volume
     *
     * @param setpoint  The temperature setpoint in ºC
     * @param volume_ul The max volume in the plate, in microliters
     * @return The overshoot setpoint that the thermocycler should target,
     *         in ºC
     */
    [[nodiscard]] static auto calculate_overshoot(double setpoint,
                                                  double volume_ul) -> double {
        if (volume_ul <= OVERSHOOT_MIN_VOLUME_MICROLITERS ||
            setpoint <= TEMPERATURE_AMBIENT) {
            return setpoint;
        }
        return setpoint + (OVERSHOOT_DEGREES_PER_MICROLITER * volume_ul) +
               OVERSHOOT_TARGET_SWITCH_DIFFERENCE;
    }

    /**
     * @brief Calculate the undershoot target temperature based off of a
     * setpoint temperature and a liquid volume
     *
     * @param setpoint  The temperature setpoint in ºC
     * @param volume_ul The max volume in the plate, in microliters
     * @return The undershoot setpoint that the thermocycler should target,
     *         in ºC
     */
    [[nodiscard]] static auto calculate_undershoot(double setpoint,
                                                   double volume_ul) -> double {
        if (volume_ul <= UNDERSHOOT_MIN_VOLUME_MICROLITERS ||
            setpoint <= TEMPERATURE_AMBIENT) {
            return setpoint;
        }
        return setpoint + (UNDERSHOOT_DEGREES_PER_MICROLITER * volume_ul) -
               OVERSHOOT_TARGET_SWITCH_DIFFERENCE;
    }

  private:
    /**
     * @brief Apply a ramp to the target temperature of an element.
     * @param[in] peltier The peltier to ramp target temperature of
     * @param[in] time The time that has passed since the last update
     * @param[in] target The target temperature to ramp towards
     */
    auto update_ramp(thermal_general::Peltier &peltier, Seconds time,
                     double target) -> void;
    /**
     * @brief Update the PID control of a single peltier
     * @param[in] peltier The peltier to update
     * @param[in] time The time that has passed since the last update
     * @return The new power value for the element
     */
    auto update_pid(thermal_general::Peltier &peltier, Seconds time) -> double;
    /**
     * @brief Update the control of the heatsink fan during active control
     * @param[in] time The time that has passed since the last update
     * @return The new power value for the fan
     */
    auto update_fan(Seconds time) -> double;
    /**
     * @brief Reset a peltier for a new setpoint. Sets the target
     * temperature to the current average plate temperature and
     * adjusts the PID to prepare for a new ramp.
     * @pre Set the new setpoint and hold time configurations
     * @param[in] peltier The peltier to reset control for.
     */
    auto reset_control(thermal_general::Peltier &peltier, double setpoint)
        -> void;
    /**
     * @brief Reset a fan for a new setpoint. Adjusts the PID
     * to prepare for a new ramp.
     * @pre Set the new setpoint and hold time configurations
     * @param[in] fan The fan to reset control for.
     */
    auto reset_control(thermal_general::HeatsinkFan &fan) -> void;

    /**
     * @brief Based on the current temperature readings, check if the average
     * temperature of the plate has crossed the setpoint
     *
     * @param heating If true, check if the channels are above the target.
     *                Otherwise checks if they are below the target.
     * @return true if all temperatures have crossed the setpoint
     */
    [[nodiscard]] auto crossed_setpoint(bool heating) const -> bool;

    /**
     * @brief Checks if a single peltier channel has crossed the setpoint.
     *
     * @param channel The channel to check
     * @param heating If true, check if the channel is above the target.
     *                Otherwise checks if it is below the target.
     * @return true
     * @return false
     */
    [[nodiscard]] auto crossed_setpoint(const thermal_general::Peltier &channel,
                                        bool heating) const -> bool;
    /**
     * @brief Checks if a single peltier channel has crossed the setpoint.
     *
     * @param channel The channel to check
     * @param threshold The absolute value threshold that counts as an
     *                  allowable distance from the target
     * @return true if the channel is at its specific target
     */
    [[nodiscard]] static auto channel_at_target(
        const thermal_general::Peltier &channel, double target,
        double threshold) -> bool {
        auto temp = channel.current_temp();
        return std::abs(target - temp) < threshold;
    }

    /**
     * @brief When ramping, the center channel needs to target a further
     * setpoint than the other channels to ensure the actual plate
     * temperatures are uniform. This is due to the differing thermal
     * mass of the different points in the well.
     *
     * @param setpoint The target setpoint
     * @param heating True if this is a heat action, false otherwise
     * @return double
     */
    [[nodiscard]] static auto center_channel_target(double setpoint,
                                                    bool heating) -> double {
        static constexpr double CENTER_EXTRA_HEATING = 1.5;
        static constexpr double CENTER_EXTRA_COOLING = 3.0;
        if (setpoint < TEMPERATURE_AMBIENT) {
            return setpoint;
        }
        if (heating) {
            return setpoint + CENTER_EXTRA_HEATING;
        }
        return setpoint - CENTER_EXTRA_COOLING;
    }

    /**
     * @brief Returns the number of degrees difference from the target
     * where the controller should use full PID rather than maxing out the
     * power of the peltiers.
     *
     * @param pid The PID controller to calculate the band from
     * @return The number of degrees to the target temperature where the
     * controller should use full PID rather than just maxing out the power.
     */
    [[nodiscard]] static auto proportional_band(PID &pid) -> double {
        if (pid.kp() == 0.0F) {
            return 0.0F;
        }
        return PROPORTIONAL_BAND_EXTRA_FACTOR / pid.kp();
    }

    [[nodiscard]] static auto moving_away_from_ambient(double current,
                                                       double target) -> bool {
        auto target_from_ambient = target - TEMPERATURE_AMBIENT;
        auto current_from_ambient = current - TEMPERATURE_AMBIENT;
        // If the new target crosses ambient, we are moving away
        if ((target_from_ambient * current_from_ambient) < 0) {
            return true;
        }
        return std::abs(target_from_ambient) > std::abs(current_from_ambient);
    }

    PlateStatus _status = PlateStatus::STEADY_STATE;  // State machine for plate
    thermal_general::Peltier &_left;
    thermal_general::Peltier &_right;
    thermal_general::Peltier &_center;
    thermal_general::HeatsinkFan &_fan;

    // Adjusted setpoint based on overshoot status
    double _current_setpoint = 0.0F;
    double _setpoint = 0.0F;  // User-provided setpoint
    double _ramp_rate = 0.0F;
    // Once the plate is in the "steady state" mode, this timer tracks
    // how long until the firmware should check for uniformity errors.
    Seconds _uniformity_error_timer = 0.0F;
    Seconds _hold_time = 0.0F;            // Total hold time
    Seconds _remaining_hold_time = 0.0F;  // Hold time left, out of _hold_time
};

}  // namespace plate_control
