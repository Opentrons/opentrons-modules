#pragma once

#include <cstdint>

#include "thermocycler-refresh/errors.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
#include "firmware/motor_hardware.h"
#pragma GCC diagnostic pop

class MotorPolicy {
  public:
    auto lid_stepper_set_vref(uint16_t target_vref_mV) -> void;
    auto lid_stepper_start(float angle) -> void;
    auto lid_stepper_stop() -> void;
    auto lid_stepper_check_fault() -> bool;
    auto lid_stepper_reset() -> bool;
    auto lid_solenoid_disengage() -> void;
    auto lid_solenoid_engage() -> void;

    auto delay_ticks(uint16_t ticks) -> void;
};
