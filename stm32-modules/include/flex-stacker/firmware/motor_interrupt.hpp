#pragma once

#include "firmware/motor_hardware.h"
#include "firmware/motor_policy.hpp"
#include "systemwide.h"

namespace motor_interrupt_controller {

using MotorPolicy = motor_policy::MotorPolicy;

static constexpr int TIMER_FREQ = 100000;
static constexpr int DEFAULT_MOTOR_FREQ = 50;

class MotorInterruptController {
  public:
    MotorInterruptController(MotorID id, MotorPolicy* policy)
        : _id(id), _policy(policy), _initialized(false) {}
    auto tick() -> void {
        step_count = (step_count + 1) % (TIMER_FREQ / step_freq);
        if (step_count == 0) {
            _policy->step(_id);
        }
    }
    auto set_freq(uint32_t freq) -> void { step_freq = freq; }
    auto initialize(MotorPolicy* policy) -> void {
        _policy = policy;
    }

  private:
    MotorID _id;
    MotorPolicy* _policy;
    bool _initialized;
    uint32_t step_count = 0;
    uint32_t step_freq = DEFAULT_MOTOR_FREQ;
};

}  // namespace motor_interrupt_controller
