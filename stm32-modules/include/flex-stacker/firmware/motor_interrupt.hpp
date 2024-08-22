#pragma once

#include "firmware/motor_hardware.h"
#include "systemwide.h"

namespace motor_interrupt_controller {

static constexpr int TIMER_FREQ = 100000;
static constexpr int DEFAULT_MOTOR_FREQ = 50;

class MotorInterruptController {
  public:
    MotorInterruptController(MotorID m_id) : m_id(m_id) {}
    auto tick() -> void {
        step_count = (step_count + 1) % (TIMER_FREQ / step_freq);
        if (step_count == 0) {
            step_motor(m_id);
        }
    }
    auto set_freq(uint32_t freq) -> void { step_freq = freq; }

  private:
    MotorID m_id;
    uint32_t step_count = 0;
    uint32_t step_freq = DEFAULT_MOTOR_FREQ;
};

}  // namespace motor_interrupt_controller
