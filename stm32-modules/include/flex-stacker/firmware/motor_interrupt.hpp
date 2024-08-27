#pragma once

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
    MotorInterruptController(MotorID id, MotorPolicy* policy)
        : _id(id),
          _policy(policy),
          _initialized(false),
          _profile(TIMER_FREQ, 0, 0, 0, motor_util::MovementType::OpenLoop, 0) {
    }

    auto tick() -> bool {
        if (!_initialized) {
            return false;
        }
        auto ret = _profile.tick();
        if (ret.step) {
            _policy->step(_id);
        }
        if (ret.done) {
            _policy->disable_motor(_id);
        }
        return ret.done;
    }
    auto set_freq(uint32_t freq) -> void { step_freq = freq; }
    auto initialize(MotorPolicy* policy) -> void { _policy = policy; }
    auto start_movement(uint32_t id, long steps, uint32_t frequency) -> void {
        _profile = motor_util::MovementProfile(
            TIMER_FREQ, 0, frequency, 0,
            motor_util::MovementType::FixedDistance, steps);
        _policy->enable_motor(_id);
        _response_id = id;
    }
    auto set_direction(bool direction) -> void {
        _policy->set_direction(_id, direction);
    }

    [[nodiscard]] auto get_response_id() const -> uint32_t {
        return _response_id;
    }

  private:
    MotorID _id;
    MotorPolicy* _policy;
    bool _initialized;
    motor_util::MovementProfile _profile;
    uint32_t step_count = 0;
    uint32_t step_freq = DEFAULT_MOTOR_FREQ;
    uint32_t _response_id = 0;
};

}  // namespace motor_interrupt_controller
