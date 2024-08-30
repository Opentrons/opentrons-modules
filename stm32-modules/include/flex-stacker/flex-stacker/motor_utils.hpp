#pragma once

#include <algorithm>
#include <concepts>

#include "core/fixed_point.hpp"

namespace motor_util {

enum class Parameter : char {
    Velocity = 'V',
    Acceleration = 'A',
    RunCurrent = 'R',
    HoldCurrent = 'H'
};

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
    auto tick() -> TickReturn __attribute__((optimize(3)));

    /** Returns the current motor velocity in steps_per_tick.*/
    [[nodiscard]] auto current_velocity() const -> steps_per_tick;

    /** Returns the target number of ticks for this movement.*/
    [[nodiscard]] auto target_distance() const -> ticks;

    /** Returns the number of ticks that have been taken.*/
    [[nodiscard]] auto current_distance() const -> ticks;

    /** Returns the movement type.*/
    [[nodiscard]] auto movement_type() const -> MovementType;

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
