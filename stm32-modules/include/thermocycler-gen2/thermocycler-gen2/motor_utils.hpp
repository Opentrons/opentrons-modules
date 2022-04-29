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
    /** Possible states of the lid stepper.*/
    enum class Position { BETWEEN, CLOSED, OPEN, UNKNOWN };

    [[nodiscard]] constexpr static auto status_to_string(Position status)
        -> const char* {
        switch (status) {
            case Position::BETWEEN:
                return "in_between";
                break;
            case Position::CLOSED:
                return "closed";
                break;
            case Position::OPEN:
                return "open";
                break;
            case Position::UNKNOWN:
                return "unknown";
                break;
        }
        return "unknown";
    }

    /**
     * @brief Convert a current value in milliamperes to a DAC value.
     *
     * @param mamps Milliampere drive value to set for the current control
     * @return uint8_t containing the DAC register value for this voltage
     */
    [[nodiscard]] constexpr static auto current_to_dac(double mamps)
        -> uint8_t {
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
    [[nodiscard]] constexpr static auto angle_to_microsteps(double angle)
        -> int32_t {
        return static_cast<int32_t>(angle * DEGREES_TO_MICROSTEPS);
    }
};

class SealStepper {
  public:
    /** Enumeration of supported parameters.*/
    enum class Parameter : char {
        Velocity = 'V',
        Acceleration = 'A',
        StallguardThreshold = 'T',
        StallguardMinVelocity = 'M',
        RunCurrent = 'R',
        HoldCurrent = 'H'
    };

    /** Possible status of the seal stepper.*/
    enum class Status { BETWEEN, ENGAGED, RETRACTED, UNKNOWN };

    // 16MHz external oscillator
    static constexpr const double tmc_external_clock = 16000000;

    [[nodiscard]] static auto status_to_string(Status status) -> const char* {
        switch (status) {
            case Status::BETWEEN:
                return "in_between";
                break;
            case Status::ENGAGED:
                return "engaged";
                break;
            case Status::RETRACTED:
                return "retracted";
                break;
            case Status::UNKNOWN:
                return "unknown";
                break;
        }
        return "unknown";
    }

    /**
     * @brief Convert a velocity into a period value
     *
     * @param velocity The velocity in steps/second
     * @param clock Clock rate of the TMC2130. Default to \ref
     * tmc_external_clock
     * @return uint32_t containing the number of \c clock ticks per each motor
     * step
     */
    [[nodiscard]] constexpr static auto inline velocity_to_tstep(
        double velocity, double clock = tmc_external_clock) -> uint32_t {
        return static_cast<uint32_t>(clock / velocity);
    }

    /**
     * @brief Convert a period of TMC2130 clock ticks into a velocity in
     * steps/sec
     *
     * @param tstep The period to convert, in \c clock ticks per each motor step
     * @param clock Clock rate of the TMC2130. Default to \ref
     * tmc_external_clock
     * @return double containing the velocity in steps/second
     */
    [[nodiscard]] constexpr static auto inline tstep_to_velocity(
        uint32_t tstep, double clock = tmc_external_clock) -> double {
        // Avoid divide-by-zero issues, bound tstep to at least 1
        tstep = std::max(tstep, static_cast<uint32_t>(1));
        return clock / static_cast<double>(tstep);
    }
};

/** The end condition for this movement.*/
enum class MovementType {
    FixedDistance,  // This movement goes for a fixed number of steps.
    OpenLoop,       // This movement goes until a stop switch is hit
};

/**
 * @brief Encapsulates information about a motor movement profile, and
 * generates information about when steps should occur and when the
 * movement should end based on a periodic \c tick() function.
 *
 * @details
 * The \c tick() function should be invoked at a fixed frequency, defined
 * in the constructor. With each tick, the MovementProfile will:
 *
 *   1. Accelerate the velocity, if the peak acceleration isn't reached \n
 *   2. Return \c step=true if a motor step should occur \n
 *   3. Return \c done=true if the movement is over (has reached the requested
 *      number of steps and is a \c FixedDistance movement)
 *
 * This class does \e not directly call any functions to move the motor. The
 * caller of \c tick() should handle actual signal generation based off of the
 * return values.
 *
 */
class MovementProfile {
  public:
    using ticks = uint64_t;
    using steps_per_tick = sq0_31;
    using steps_per_tick_sq = sq0_31;

    // Radix for all fixed point values
    static constexpr int radix = 31;

    struct TickReturn {
        bool done;  // If true, this movement is done
        bool step;  // If true, motor should step
    };

    /**
     * @brief Construct a new Movement Profile object
     *
     * @param[in] ticks_per_second Frequency of the motor interrupt
     * @param[in] start_velocity Starting velocity in steps per second
     * @param[in] peak_velocity Max velocity in steps per second
     * @param[in] acceleration Acceleration in steps per second^2. Set to 0
     *                         or lower for instant acceleration.
     * @param[in] type The type of movement to perform. A FixedDistance
     *                 movement will have no deceleration profile.
     * @param[in] distance The number of ticks to move. Irrelevant for
     *                     OpenLoop movements.
     */
    MovementProfile(uint32_t ticks_per_second, double start_velocity,
                    double peak_velocity, double acceleration,
                    MovementType type, ticks distance);

    auto reset() -> void;

    /**
     * @brief Call this function for every timer interrupt signalling a tick,
     * which should be at the rate \c ticks_per_second
     * @note If called after a movement is completed, steps will keep being
     * generated. The caller should monitor the return value to know when
     * to stop calling tick()
     *
     * @return TickReturn with information for how hardware driver should act
     */
    auto tick() -> TickReturn;

    /** Returns the current motor velocity in steps_per_tick.*/
    [[nodiscard]] auto current_velocity() const -> steps_per_tick;

    /** Returns the target number of ticks for this movement.*/
    [[nodiscard]] auto target_distance() const -> ticks;

    /** Returns the number of ticks that have been taken.*/
    [[nodiscard]] auto current_distance() const -> ticks;

  private:
    uint32_t _ticks_per_second;           // Tick frequency
    steps_per_tick _velocity = 0;         // Current velocity
    steps_per_tick _start_velocity = 0;   // Velocity to start a movement
    steps_per_tick _peak_velocity = 0;    // Velocity to ramp up to
    steps_per_tick_sq _acceleration = 0;  // Acceleration in steps/tick^2
    MovementType _type;                   // Type of movement
    ticks _target_distance;               // Distance for the movement
    ticks _current_distance = 0;          // Distance this movement has reached
    q31_31 _tick_tracker = 0;             // Running tracker for the tick motion

    // When incrementing position tracker, if this bit changes then
    // a step should take place.
    static constexpr q31_31 _tick_flag = (1 << radix);
};

}  // namespace motor_util
