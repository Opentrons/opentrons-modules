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
    /**
     * @brief Set the value of the DAC as a register value. The DAC is used
     * to control the drive current of the lid stepper.
     * @param dac_val Value to set the DAC to
     */
    auto lid_stepper_set_dac(uint8_t dac_val) -> void;
    /**
     * @brief Start a lid stepper movement as a relative movement
     *
     * @param steps Number of steps to move. Can be positive or negative
     * to indicate direction.
     */
    auto lid_stepper_start(int32_t steps) -> void;
    /**
     * @brief Stop any movement on the lid stepper.
     *
     */
    auto lid_stepper_stop() -> void;
    /**
     * @brief Check if a fault is present in the lid stepper driver
     *
     * @return true if a fault is raised, false otherwise
     */
    auto lid_stepper_check_fault() -> bool;
    /**
     * @brief Reset the lid stepper driver, clearing the fault flag
     *
     * @return true if a fault is seen \e after reset, false otherwise
     */
    auto lid_stepper_reset() -> bool;
    /**
     * @brief Disengage the lid solenoid
     */
    auto lid_solenoid_disengage() -> void;
    /**
     * @brief Engage the lid solenoid
     */
    auto lid_solenoid_engage() -> void;
};
