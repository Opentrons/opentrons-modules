#pragma once
#include <atomic>

#include "firmware/motor_hardware.h"
#include "firmware/motor_policy.hpp"
#include "flex-stacker/motor_utils.hpp"
#include "systemwide.h"

namespace motor_interrupt_controller {

using MotorPolicy = motor_policy::MotorPolicy;

static constexpr int TIMER_FREQ = 100000;
static constexpr int DEFAULT_MOTOR_FREQ = 50;

static constexpr double DEFAULT_VELOCITY = 64000;  // steps per second
static constexpr double DEFAULT_ACCEL = 50000;     // steps per second^2

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
        if (ret.done) {
            _policy->disable_motor(_id);
        }
        return ret.done;
    }
    auto set_freq(uint32_t freq) -> void { step_freq = freq; }
    auto initialize(MotorPolicy* policy) -> void {
        _policy = policy;
        _initialized = true;
    }
    auto start_movement(uint32_t move_id, bool direction, long steps,
                        uint32_t frequency) -> void {
        set_direction(direction);
        _profile = motor_util::MovementProfile(
            TIMER_FREQ, 0, frequency, 0,
            motor_util::MovementType::FixedDistance, steps);
        _policy->enable_motor(_id);
        _response_id = move_id;
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
    auto stop_condition_met() -> bool {
        if (_profile.movement_type() == motor_util::MovementType::OpenLoop) {
            return limit_switch_triggered();
        }
        return false;
    }

  private:
    MotorID _id;
    MotorPolicy* _policy;
    std::atomic_bool _initialized;
    motor_util::MovementProfile _profile;
    uint32_t step_count = 0;
    uint32_t step_freq = DEFAULT_MOTOR_FREQ;
    uint32_t _response_id = 0;
    bool _direction = false;
};

}  // namespace motor_interrupt_controller
