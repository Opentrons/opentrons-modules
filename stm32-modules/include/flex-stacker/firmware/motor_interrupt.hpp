#pragma once
#include <atomic>
#include <cstdint>

#include "firmware/motor_hardware.h"
#include "firmware/motor_policy.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/motor_utils.hpp"
#include "systemwide.h"

namespace motor_interrupt_controller {

using MotorPolicy = motor_policy::MotorPolicy;
using Error = errors::ErrorCode;

static constexpr int TIMER_FREQ = 100000;

struct Move {
    MotorID motor_id;
    motor_util::MotorState* motor_state;
    uint32_t move_id;
    bool direction;
    float distance;
    bool limit_switch;
    bool has_next_move;
};

class MotorInterruptController {
  public:
    explicit MotorInterruptController(MotorID id, MotorPolicy* policy)
        : _id(id),
          _policy(policy),
          _initialized(false),
          _profile(TIMER_FREQ, 0, 0, 0, motor_util::MovementType::OpenLoop, 0) {
    }
    MotorInterruptController(MotorInterruptController const&) = delete;
    void operator=(MotorInterruptController const&) = delete;
    MotorInterruptController(MotorInterruptController const&&) = delete;
    void operator=(MotorInterruptController const&&) = delete;
    ~MotorInterruptController() = default;

    auto tick() -> bool {
        if (!_initialized) {
            return false;
        }
        auto ret = _profile.tick();
        if (ret.step && !stop_condition_met()) {
            _policy->step(_id);
        }
        if (ret.done || stop_condition_met()) {
            _policy->stop_motor(_id);
            _stop = true;
            return true;
        }
        return ret.done;
    }
    auto initialize(MotorPolicy* policy) -> void {
        _policy = policy;
        _initialized = true;
    }

    auto reset() -> void {
        _stop = false;
        _error = Error::NO_ERROR;
    }

    auto start_fixed_movement(uint32_t move_id, bool direction, long steps,
                              uint32_t steps_per_sec_discont,
                              uint32_t steps_per_sec, uint32_t step_per_sec_sq)
        -> void {
        reset();
        set_direction(direction);
        _profile = motor_util::MovementProfile(
            TIMER_FREQ, steps_per_sec_discont, steps_per_sec, step_per_sec_sq,
            motor_util::MovementType::FixedDistance, steps);
        _policy->enable_motor(_id);
        _response_id = move_id;
        _policy->start_motor_timer(_id);
    }
    auto start_move(Move move) -> void {
        motor_util::MotorState* state = move.motor_state;
        reset();
        _policy->enable_motor(_id);
        set_direction(move.direction);
        _profile = motor_util::MovementProfile(
            TIMER_FREQ, state->get_speed_discont(),
            // if moving to limit switch, use max speed discont
            move.limit_switch ? state->get_speed_discont() : state->get_speed(),
            state->get_accel(),
            move.limit_switch ? motor_util::MovementType::OpenLoop
                              : motor_util::MovementType::FixedDistance,
            state->get_distance(move.distance));
        _response_id = move.move_id;
        _policy->start_motor_timer(_id);
    }

    auto stop_movement(Error error, bool disable_motor) -> void {
        _stop = true;
        _error = error;
        _policy->stop_motor(_id);
        if (disable_motor) {
            _policy->disable_motor(_id);
        }
    }

    auto set_direction(bool direction) -> void {
        _policy->set_direction(_id, direction);
        _direction = direction;
    }
    auto limit_switch_triggered() -> bool {
        return _policy->check_limit_switch(_id, _direction);
    }

    [[nodiscard]] auto get_response_id() const -> uint32_t {
        return _response_id;
    }
    [[nodiscard]] auto get_error_code() const -> Error { return _error; }
    auto stop_condition_met() -> bool {
        // this will only error if the limit switch in the move direction
        // is triggered
        if (limit_switch_triggered()) {
            if (_profile.movement_type() ==
                motor_util::MovementType::FixedDistance) {
                _error = Error::UNEXPECTED_LIMIT_SWITCH;
            }
            return true;
        }
        if (_stop) {
            return true;
        }
        return false;
    }

    auto set_diag0_irq(bool enable) -> void { _policy->set_diag0_irq(enable); }

    [[nodiscard]] auto is_moving() const -> bool { return !_stop; }

  private:
    MotorID _id;
    MotorPolicy* _policy;
    std::atomic_bool _initialized;
    motor_util::MovementProfile _profile;
    uint32_t _response_id = 0;
    Error _error = Error::NO_ERROR;
    bool _direction = false;
    bool _stop = true;
};

}  // namespace motor_interrupt_controller
