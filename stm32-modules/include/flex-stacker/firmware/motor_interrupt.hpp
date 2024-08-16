#pragma once

#include "firmware/motor_hardware.h"
#include "systemwide.h"

namespace motor_interrupt_controller {

static constexpr int STEPS_PER_REV = 2000;

class MotorInterruptController {
  public:
    MotorInterruptController(MotorID m_id) : m_id(m_id) {}
    auto tick() -> void {
        unstep_motor(m_id);
        step_count = (step_count + 1) % STEPS_PER_REV;
        if (step_count == 0) {
            step_motor(m_id);
        }
    }

  private:
    MotorID m_id;
    uint32_t step_count = 0;
};

}  // namespace motor_interrupt_controller
