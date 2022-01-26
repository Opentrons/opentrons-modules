/**
 * @file motor_utils.hpp
 * @brief Provides some utility functions for motor control on the
 * Thermocycler Refresh
 *
 */

#pragma once

#include <algorithm>
#include <concepts>

#include "core/fixed_point.hpp"

namespace motor_util {

class LidStepper {
  private:
    // Max current the DAC can output, in millivolts.
    //   Max voltage = 3.3v, R = 0.05 ohms
    //   Current = V / (8 * R) = v / 0.4
    //   Max current = 3.3 / 0.4 = 8.25 amperes
    constexpr static double DAC_MAX_CURRENT = (8.25 * 1000.0);
    // Max register value for the dac
    constexpr static uint32_t DAC_MAX_VALUE = 0xFF;

    // Steps per 360 degrees
    constexpr static double STEPS_PER_REV = 200.0 / 360.0;
    // We are using 1/32 microstepping
    constexpr static double MICROSTEPPING = 32;
    // Gear ratio factor
    constexpr static double GEAR_RATIO_SCALAR = 99.5;
    // Total factor to multiply from angle to degrees
    constexpr static double DEGREES_TO_MICROSTEPS =
        STEPS_PER_REV * MICROSTEPPING * GEAR_RATIO_SCALAR;
    constexpr static double ROTATION_TO_STEPS = DEGREES_TO_MICROSTEPS * 360;

  public:
    /**
     * @brief Convert a current value in milliamperes to a DAC value.
     *
     * @param mamps Milliampere drive value to set for the current control
     * @return uint8_t containing the DAC register value for this voltage
     */
    static auto current_to_dac(double mamps) -> uint8_t {
        // hardware has a 1ohm sense resistor and the driver has an implicit 10x
        // divider. the dac can express a max of 3.3V, so the maximum current we
        // can drive is 330mA at 3.3V/dac fullscale of 255. we can therefore
        // clamp the current input to 330 current_ma =
        //    ((current_ma > MAX_SOLENOID_CURRENT_MA) ? MAX_SOLENOID_CURRENT_MA
        //                                            : current_ma);
        // and then rescale into 8 bits with 330 ending up at 255
        auto dac_intermediate =
            std::min(mamps, DAC_MAX_CURRENT) * DAC_MAX_VALUE;
        return static_cast<uint8_t>(
            static_cast<uint32_t>(dac_intermediate / DAC_MAX_CURRENT) &
            DAC_MAX_VALUE);
    }

    /**
     * @brief Convert an angle to a number of microsteps
     *
     * @param angle The angle in degrees. Can be positive or negative
     * @return int32_t count of steps to reach this angle
     */
    static auto angle_to_microsteps(double angle) -> int32_t {
        return static_cast<int32_t>(angle * DEGREES_TO_MICROSTEPS);
    }
};

template <uint32_t ticks_per_second>
class MovementProfile {
  public:
    using ticks = uint64_t;
    using steps_per_tick = sq0_31;
    using steps_per_tick_sq = sq0_31;

    // Frequency of ticks as a double
    static constexpr double tick_freq = static_cast<double>(ticks_per_second);
    // Radix for all fixed point values
    static constexpr int radix = 31;

    // Frequency of ticks must be a positive number
    static_assert(ticks_per_second > 0, 
                  "Frequency of movement ticks must be positive");

    /** The end condition for this movement.*/
    enum class MovementType {
        FixedDistance,  // This movement goes for a fixed number of steps.
        SoftStop,       // This movement goes until a stop switch is hit
    };

    struct TickReturn {
        bool done;  // If true, this movement is done
        bool step;  // If true, motor should step
    };

    /**
     * @brief Construct a new Movement Profile object
     * 
     * @param[in] start_velocity Starting velocity in steps per second
     * @param[in] peak_velocity Max velocity in steps per second
     * @param[in] end_velocity Ending velocity in steps per second
     * @param[in] acceleration Acceleration in steps per second^2. Set to 0
     *                         for instant acceleration.
     * @param[in] deceleration Deceleration in steps per second^2. Set to 0
     *                         for instant deceleration.
     * @param[in] type The type of movement to perform. A FixedDistance 
     *                 movement will have no deceleration profile.
     * @param[in] distance The number of ticks to move. Irrelevant for
     *                     SoftStop movements.
     */
    MovementProfile(double start_velocity, double peak_velocity, 
                    double acceleration, MovementType type,
                    ticks distance) :
            _type(type),
            _target_distance(distance) {
        _start_velocity = convert_to_fixed_point(start_velocity / tick_freq, radix);
        _peak_velocity = convert_to_fixed_point(peak_velocity / tick_freq, radix);
        _acceleration = convert_to_fixed_point(acceleration / (tick_freq * tick_freq), radix);

        if(_acceleration == 0) {
            _start_velocity = _peak_velocity;
        }

        reset();
    }

    auto reset() -> void {
        _velocity = _start_velocity;
        _current_distance = 0;
        _tick_tracker = 0;
    }

    /**
     * @brief Call this function for every timer interrupt signalling a tick,
     * which should be at the rate \c ticks_per_second
     * @note If called after a movement is completed, steps will keep being
     * generated. The caller should monitor the return value to know when
     * to stop calling tick()
     * 
     * @return TickReturn with information for how hardware driver should act
     */
    auto tick() -> TickReturn {
        bool step = false;
        // Acceleration
        if(_velocity < _peak_velocity) {
            _velocity += _acceleration;
            if(_velocity > _peak_velocity) {
                _velocity = _peak_velocity;
            }
        }
        auto old_tick_track = _tick_tracker;
        _tick_tracker += _velocity;
        // The bit _tick_flag represents a "whole" step. Once this flips,
        // the code signals that a step should occur
        if((old_tick_track ^ _tick_tracker) & _tick_flag) {
            step = true;
        }
        return TickReturn{.done = (_current_distance >= _target_distance), .step = step};
    }

  private:
    steps_per_tick _velocity = 0; // Current velocity
    steps_per_tick _start_velocity = 0; // Velocity to start a movement
    steps_per_tick _peak_velocity = 0; // Velocity to ramp up to
    steps_per_tick_sq _acceleration = 0; // Acceleration in steps/tick^2
    MovementType _type; // Type of movement
    ticks _target_distance; // Distance for the movement
    ticks _current_distance = 0; // Distance this movement has reached
    q31_31 _tick_tracker = 0; // Running tracker for the tick motion

    // When incrementing position tracker, if this bit changes then
    // a step should take place.
    static constexpr q31_31 _tick_flag = (1 << radix);
};

}  // namespace motor_util
