#include "flex-stacker/motor_utils.hpp"

using namespace motor_util;

MovementProfile::MovementProfile(uint32_t ticks_per_second,
                                 double start_velocity, double peak_velocity,
                                 double acceleration, MovementType type,
                                 ticks distance)
    : _ticks_per_second(ticks_per_second),
      _type(type),
      _target_distance(distance) {
    // Clamp ticks_per_second to at least 1
    _ticks_per_second = std::max(_ticks_per_second, static_cast<uint32_t>(1));

    auto tick_freq = static_cast<double>(_ticks_per_second);

    // Clamp the peak velocity so it is not under start velocity
    start_velocity = std::max(start_velocity, static_cast<double>(0.0F));
    acceleration = std::max(acceleration, static_cast<double>(0.0F));
    peak_velocity = std::max(start_velocity, peak_velocity);

    // Convert velocities by just dividing by the tick frequency
    _start_velocity = convert_to_fixed_point(start_velocity / tick_freq, radix);
    _peak_velocity = convert_to_fixed_point(peak_velocity / tick_freq, radix);
    // Acceleration must be dividied by (tick/sec)^2 for unit conversion
    _acceleration =
        convert_to_fixed_point(acceleration / (tick_freq * tick_freq), radix);

    if (_acceleration <= 0) {
        _start_velocity = _peak_velocity;
    }

    // Ensures that all movement variables are initialized properly
    reset();
}

auto MovementProfile::reset() -> void {
    // seems like we'd have miss the distance traveled by (0->start velocity)?
    _velocity = _start_velocity;
    _current_distance = 0;
    _tick_tracker = 0;
    _accel_distance = 0;
}

auto MovementProfile::tick() -> TickReturn {
    bool step = false;
    if (_type == MovementType::FixedDistance) {
        // Acceleration phase (until half of the profile is reached)
        if (_velocity < _peak_velocity &&
            _current_distance < _target_distance / 2) {
            _velocity += _acceleration;
            if (_velocity > _peak_velocity) {
                _velocity = _peak_velocity;
                // we're done accelerating
                _accel_distance = _current_distance;
            }
        } else {
            if (_accel_distance == 0) {
                _accel_distance = _current_distance;
            }
            if (remaining_distance() <= _accel_distance) {
                // Deceleration phase (after more than half of the target
                // distance is covered)
                _velocity -= _acceleration;
                if (_velocity < _start_velocity) {
                    _velocity = _start_velocity;
                }
            }
        }
    } else {
        // TODO: re-evaluate if we need to accelerate during open loop movement
        // Acceleration gets clamped to _peak_velocity
        if (_velocity < _peak_velocity) {
            _velocity += _acceleration;
            if (_velocity > _peak_velocity) {
                _velocity = _peak_velocity;
            }
        }
    }

    auto old_tick_track = _tick_tracker;
    _tick_tracker += _velocity;
    // The bit _tick_flag represents a "whole" step. Once this flips,
    // the code signals that a step should occur
    if (((old_tick_track ^ _tick_tracker) & _tick_flag) != 0U) {
        step = true;
        ++_current_distance;
    }
    return TickReturn{.done = (_current_distance >= _target_distance &&
                               _type == MovementType::FixedDistance),
                      .step = step};
}

[[nodiscard]] auto MovementProfile::current_velocity() const -> steps_per_tick {
    return _velocity;
}

/** Returns the number of total number of ticks that should be taken.*/
[[nodiscard]] auto MovementProfile::target_distance() const -> ticks {
    return _target_distance;
}

/** Returns the number of ticks that have been taken.*/
[[nodiscard]] auto MovementProfile::current_distance() const -> ticks {
    return _current_distance;
}

/** Returns the movement type.*/
[[nodiscard]] auto MovementProfile::movement_type() const -> MovementType {
    return _type;
}

/** Returns the number of ticks yet to be taken.*/
[[nodiscard]] auto MovementProfile::remaining_distance() const -> ticks {
    return _target_distance - _current_distance;
}
