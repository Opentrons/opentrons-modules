#pragma once

#include <cstdint>

#include "firmware/motor_hardware.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/tmc2130.hpp"

class MotorPolicy {
    std::function<void()> _seal_callback;

  public:
    using RxTxReturn = std::optional<tmc2130::MessageT>;

    /** Frequency of the seal motor interrupt in hertz.*/
    static constexpr const uint32_t MotorTickFrequency = MOTOR_INTERRUPT_FREQ;

    /**
     * @brief Construct a new Motor Policy object
     *
     */
    MotorPolicy()
        : _seal_callback() {}  // NOLINT(readability-redundant-member-init)

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

    /**
     * @brief Start a new seal stepper movement
     *
     * @param callback Function to call on every tick
     * @return True if seal stepper could be started, false otherwise
     */
    auto seal_stepper_start(std::function<void()> callback) -> bool;
    /**
     * @brief Stop any active seal stepper movement
     */
    auto seal_stepper_stop() -> void;

    /**
     * @brief Send and receive data over SPI to tmc2130
     *
     * @param data Message to send to the TMC2130
     * @return RxTxReturn containing received data, or no value if
     * transmission failed
     */
    auto tmc2130_transmit_receive(tmc2130::MessageT& data) -> RxTxReturn;
    /**
     * @brief Set the enable pin for the TMC2130 to enabled or not
     *
     * @param enable True to enable the output, false to disable
     * @return true on success, false on error
     */
    auto tmc2130_set_enable(bool enable) -> bool;
    /**
     * @brief Set the direction pin for the TMC2130 to forward
     * or back
     *
     * @param direction True to go forwards, false for backwards
     * @return true on success, false on error
     */
    auto tmc2130_set_direction(bool direction) -> bool;
    /**
     * @brief Pulse the step pin on the TMC2130
     *
     * @return true on success, false on error
     */
    auto tmc2130_step_pulse() -> bool;

    /**
     * @brief Call the seal callback function
     *
     */
    auto seal_tick() -> void { _seal_callback(); }
};
