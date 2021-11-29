#pragma once

#include <cstdint>

#include "thermocycler-refresh/errors.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "motor_hardware.h"
#include "stm32g4xx_hal.h"
#pragma GCC diagnostic pop

class MotorPolicy {
  public:
    MotorPolicy() = delete;
    explicit MotorPolicy(motor_hardware_handles* handles);

    auto lid_solenoid_disengage() -> void;
    auto lid_solenoid_engage() -> void;

    auto delay_ticks(uint16_t ticks) -> void;

  private:
    motor_hardware_handles* hw_handles;
};
