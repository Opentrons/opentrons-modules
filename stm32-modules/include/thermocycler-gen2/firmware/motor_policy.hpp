#pragma once

#include <cstdint>

#include "firmware/motor_hardware.h"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/tmc2130.hpp"

/**
 * @brief Policy implementation for motor hardware.
 *
 * This policy is compatible with two schemes for the seal limit switches, set
 * by _shared_seal_switch_lines.
 *
 * If the lines are shared, then only the line for the extension switch is
 * actually used. This applies to board revisions below REV3.
 *
 * If the lines are NOT shared, then two different GPIO lines are used. This
 * applies to all boards at REV3 and above.
 *
 */
class MotorPolicy {
    std::function<void()> _seal_callback;
    bool _shared_seal_switch_lines;

  public:
    using RxTxReturn = std::optional<tmc2130::MessageT>;

    /** Frequency of the seal motor interrupt in hertz.*/
    static constexpr const uint32_t MotorTickFrequency = MOTOR_INTERRUPT_FREQ;

    explicit MotorPolicy() = delete;

    /**
     * @brief Construct a new Motor Policy object
     *
     */
    MotorPolicy(bool shared_seal_switch_lines);

    MotorPolicy(MotorPolicy&& other);
    MotorPolicy(const MotorPolicy& other);
    MotorPolicy& operator=(MotorPolicy&& other);
    MotorPolicy& operator=(const MotorPolicy& other);

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
     * @param overdrive True to ignore the endstop switches for this movement
     */
    auto lid_stepper_start(int32_t steps, bool overdrive) -> void;
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
     * @brief Read whether the Closed Switch is active
     *
     * @return true if the lid is closed, false otherwise
     */
    auto lid_read_closed_switch() -> bool;

    /**
     * @brief Read whether the Open Switch is active
     *
     * @return true if the lid is fully open, false otherwise
     */
    auto lid_read_open_switch() -> bool;

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
     * @brief Arm the extension limit switch for the seal motor.
     */
    auto seal_switch_set_extension_armed() -> void;

    /**
     * @brief Arm the retraction limit switch for the seal motor.
     */
    auto seal_switch_set_retraction_armed() -> void;

    /**
     * @brief Disarm the limit switches for the seal motor.
     *
     */
    auto seal_switch_set_disarmed() -> void;

    /**
     * @brief Read the seal's extension limit switch
     *
     * @return true if the switch is triggered, false otherwise
     */
    auto seal_read_extension_switch() -> bool;

    /**
     * @brief Read the seal's retraction limit switch
     *
     * @return true if the switch is triggered, false otherwise
     */
    auto seal_read_retraction_switch() -> bool;

    /**
     * @brief Call the seal callback function
     *
     */
    auto seal_tick() -> void { _seal_callback(); }
};
