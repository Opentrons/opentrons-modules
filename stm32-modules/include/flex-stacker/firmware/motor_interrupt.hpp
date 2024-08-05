#pragma once

#include "firmware/motor_hardware.h"
#include "systemwide.h"

namespace motor_interrupt_controller {

class MotorInterruptController {
  public:
    MotorInterruptController(MotorID m_id): m_id(m_id) {}
    void tick() {
        step_count = (step_count + 1) % 200000;
        if (step_count == 0) {
            step_motor(m_id);
        }
    }
  private:
    MotorID m_id;
    uint32_t step_count = 0;
};

}  // namespace motor_driver_policy