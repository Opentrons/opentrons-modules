/*
 * the primary interface to the motor control task
 */
#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <functional>
#include <optional>
#include <variant>

#include "hal/message_queue.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/motor_utils.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "thermocycler-refresh/tmc2130.hpp"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace motor_task {

/*
 * The MotorExecutionPolicy is how the portable task interacts
 * with the hardware. It is defined as a concept so it can be
 * passed as a reference paramter to run_once(), which means the
 * type of policy in actual use does not have to be part of the class's
 * type signature (which is used all over the place), just run_once's
 * type signature, which is used just by the rtos task and the test
 * harness.
 *
 * The policy exposes methods to get relevant data from the motor hardware
 * and methods to change the state of the motor controller.
 *
 * The policy is not the only way in which the hardware may interact
 * with the motor controller; it may also send messages. This should
 * be the way that the hardware sends information to the motor task
 * (as opposed to the motor task querying information from the hardware).
 * For instance, an asynchronous error mechanism should inform the motor
 * task of its event by sending a message.
 */
template <typename Policy>
concept MotorExecutionPolicy = requires(Policy& p,
                                        std::function<void()> callback) {
    // A function to set the stepper DAC as a register value
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.lid_stepper_set_dac(1)};
    // A function to start a stepper movement. Accepts a number of steps,
    // and a boolean argument for whether this is an overdrive movement.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.lid_stepper_start(1, true)};
    // A function to stop a stepper movement
    {p.lid_stepper_stop()};
    // A function to check for a fault in the stepper movement
    { p.lid_stepper_check_fault() } -> std::same_as<bool>;
    // A function to reset the stepper driver
    {p.lid_stepper_reset()};
    // A function to disengage the solenoid
    {p.lid_solenoid_disengage()};
    // A function to engage the solenoid
    {p.lid_solenoid_engage()};
    // A function to read the Lid Closed Switch
    { p.lid_read_closed_switch() } -> std::same_as<bool>;
    // A function to read the Lid Open Switch
    { p.lid_read_open_switch() } -> std::same_as<bool>;
    // A function to start a seal stepper movement, with a callback for each
    // tick
    { p.seal_stepper_start(callback) } -> std::same_as<bool>;
    // A function to stop a seal stepper movement
    {p.seal_stepper_stop()};
    // Policy defines a number that provides the number of seal motor ticks
    // in a second
    {std::is_integral_v<decltype(Policy::MotorTickFrequency)>};
}
&&tmc2130::TMC2130Policy<Policy>;

// Structure to encapsulate state of the lid stepper
struct LidStepperState {
    // Full open/close movements run until they hit an endstop switch, so the
    // distance is 120 degrees which is far wider than the actual travel angle.
    constexpr static double FULL_OPEN_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(120);
    // After opening to the open switch, the lid must close ~5º to
    // be at the 90º position
    constexpr static double OPEN_BACK_TO_90_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(-17);
    // Full open/close movements run until they hit an endstop switch, so the
    // distance is 120 degrees which is far wider than the actual travel angle.
    constexpr static double FULL_CLOSE_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(-120);
    // After closing to the switch, the lid must "overdrive" a few degrees to
    // be fully seated in the closed position. The endstop switch is ignored
    // for this movement.
    constexpr static double CLOSE_OVERDRIVE_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(-5);
    // Default run current is 1200 milliamperes
    constexpr static double DEFAULT_RUN_CURRENT =
        motor_util::LidStepper::current_to_dac(1200);
    // States for lid stepper
    enum Status {
        IDLE,            /**< Not moving.*/
        SIMPLE_MOVEMENT, /**< Single stage movement.*/
        OPEN_TO_SWITCH,  /**< Open until the open switch is hit.*/
        OPEN_BACK_TO_90, /**< Close from switch back to 90º position.*/
        CLOSE_TO_SWITCH, /**< Close lid until it hits the switch.*/
        CLOSE_OVERDRIVE  /**< Close lid a few degrees into the switch.*/
    };
    // Current status of the lid stepper. Declared atomic because
    // this flag is set & cleared by both the actual task context
    // and an interrupt context when the motor interrupt fires.
    std::atomic<Status> status;
    // When a movement is complete, respond to this ID
    uint32_t response_id;
};

// Structure to encapsulate state of the seal stepper
struct SealStepperState {
    // Enumeration of legal stepper actions
    enum Status { IDLE, MOVING };
    // Current status of the seal stepper. Declared atomic because
    // this flag is set & cleared by both the actual task context
    // and an interrupt context when the motor interrupt fires.
    std::atomic<Status> status;
    // When a movement is complete, respond to this ID
    uint32_t response_id;
};

static constexpr tmc2130::TMC2130RegisterMap default_tmc_config = {
    .gconfig = {.diag0_error = 1, .diag1_stall = 1},
    .ihold_irun = {.hold_current = 0x1,    // Approx 118mA
                   .run_current = 0b1101,  // Approx 825 mA
                   .hold_current_delay = 0b0111},
    .tpowerdown = {},
    .tcoolthrs = {.threshold = 0},
    .thigh = {.threshold = 0xFFFFF},
    .chopconf = {.toff = 0b101, .hstrt = 0b101, .hend = 0b11, .tbl = 0b10},
    .coolconf = {.sgt = 4}};

using Message = ::messages::MotorMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
  public:
    using Queue = QueueImpl<Message>;

    // Default current to set for the lid stepper, in milliamperes
    static constexpr double LID_STEPPER_DEFAULT_VOLTAGE = 1200;
    // Default current to set for lid stepper for holding, in milliamperes
    static constexpr double LID_STEPPER_HOLD_CURRENT = 0;
    // Default velocity for the seal stepper, in steps/second
    static constexpr double SEAL_STEPPER_DEFAULT_VELOCITY = 50000;
    // Default acceleration for the seal stepper, in steps/second^2
    static constexpr double SEAL_STEPPER_DEFAULT_ACCELERATION = 50000;

    explicit MotorTask(Queue& q)
        : _message_queue(q),
          _task_registry(nullptr),
          _lid_stepper_state{.status = LidStepperState::Status::IDLE,
                             .response_id = 0},
          _seal_stepper_state{.status = SealStepperState::Status::IDLE,
                              .response_id = 0},
          _tmc2130(default_tmc_config),
          // Seal movement profile is populated with mostly dummy values.
          // It is set before every movement so these are irrelevant.
          _seal_profile(1, 0, SEAL_STEPPER_DEFAULT_VELOCITY,
                        SEAL_STEPPER_DEFAULT_ACCELERATION,
                        motor_util::MovementType::OpenLoop, 0),
          _seal_velocity(SEAL_STEPPER_DEFAULT_VELOCITY),
          _seal_acceleration(SEAL_STEPPER_DEFAULT_ACCELERATION),
          _seal_position(motor_util::SealStepper::Status::UNKNOWN) {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;
    auto get_message_queue() -> Queue& { return _message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        _task_registry = other_tasks;
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());

        if (!_tmc2130.initialized()) {
            _tmc2130.write_config(policy);
        }

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.
        static_cast<void>(_message_queue.recv(&message));
        std::visit(
            [this, &policy](const auto& msg) -> void {
                this->visit_message(msg, policy);
            },
            message);
    }

  private:
    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(_ignore);
        static_cast<void>(policy);
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto visit_message(const messages::LidStepperDebugMessage& msg,
                       Policy& policy) -> void {
        // check for errors
        auto error = errors::ErrorCode::NO_ERROR;
        if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            error = errors::ErrorCode::LID_MOTOR_BUSY;
        } else if (policy.lid_stepper_check_fault()) {
            error = errors::ErrorCode::LID_MOTOR_FAULT;
        }
        if (error == errors::ErrorCode::NO_ERROR) {
            // Start movement and cache the id for later
            policy.lid_stepper_set_dac(motor_util::LidStepper::current_to_dac(
                LID_STEPPER_DEFAULT_VOLTAGE));
            policy.lid_stepper_start(
                motor_util::LidStepper::angle_to_microsteps(msg.angle),
                msg.overdrive);
            _lid_stepper_state.status =
                LidStepperState::Status::SIMPLE_MOVEMENT;
            _lid_stepper_state.response_id = msg.id;
        } else {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = msg.id, .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto visit_message(const messages::LidStepperComplete& msg, Policy& policy)
        -> void {
        static_cast<void>(msg);  // No contents in message
        LidStepperState::Status old_state = _lid_stepper_state.status.load();
        auto error = handle_lid_state_end(policy);
        if (_lid_stepper_state.status == LidStepperState::Status::IDLE &&
            old_state != _lid_stepper_state.status) {
            // Send an ACK if a movement just finished
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = _lid_stepper_state.response_id,
                .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto visit_message(const messages::SealStepperDebugMessage& msg,
                       Policy& policy) -> void {
        // check for errors
        auto error = errors::ErrorCode::NO_ERROR;
        if (_seal_stepper_state.status != SealStepperState::Status::IDLE) {
            error = errors::ErrorCode::SEAL_MOTOR_BUSY;
        }
        if (error == errors::ErrorCode::NO_ERROR) {
            _seal_stepper_state.response_id = msg.id;
            error = start_seal_movement(msg.steps, policy);
        }

        // Check for error after starting movement
        if (error != errors::ErrorCode::NO_ERROR) {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = msg.id, .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto visit_message(const messages::SealStepperComplete& msg, Policy& policy)
        -> void {
        static_cast<void>(msg);
        if (_seal_stepper_state.status == SealStepperState::Status::MOVING) {
            // Ignore return in case movement was already stopped by interrupt
            static_cast<void>(policy.seal_stepper_stop());
            static_cast<void>(policy.tmc2130_set_enable(false));
            using namespace messages;
            auto with_error = errors::ErrorCode::NO_ERROR;
            switch (msg.reason) {
                case SealStepperComplete::CompletionReason::STALL:
                    // TODO: during some movements a stall is expected
                    with_error = errors::ErrorCode::SEAL_MOTOR_STALL;
                    break;
                case SealStepperComplete::CompletionReason::ERROR:
                    // TODO clear the error
                    with_error = errors::ErrorCode::SEAL_MOTOR_FAULT;
                    _seal_position = motor_util::SealStepper::Status::UNKNOWN;
                    break;
                default:
                    break;
            }
            _seal_stepper_state.status = SealStepperState::Status::IDLE;
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = _seal_stepper_state.response_id,
                .with_error = with_error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto visit_message(const messages::ActuateSolenoidMessage& msg,
                       Policy& policy) -> void {
        if (msg.engage) {
            policy.lid_solenoid_engage();
        } else {
            policy.lid_solenoid_disengage();
        }
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto visit_message(const messages::GetSealDriveStatusMessage& msg,
                       Policy& policy) -> void {
        auto ret = _tmc2130.get_driver_status(policy);
        auto response =
            messages::GetSealDriveStatusResponse{.responding_to_id = msg.id};
        if (ret.has_value()) {
            response.status = ret.value();
        }
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::SetSealParameterMessage& msg,
                       Policy& policy) -> void {
        using Parameter = motor_util::SealStepper::Parameter;

        // Only set to false if a tmc2130 write fails
        bool ret = true;
        auto with_error = errors::ErrorCode::NO_ERROR;
        switch (msg.param) {
            case Parameter::Velocity:
                _seal_velocity = static_cast<uint32_t>(
                    std::max(msg.value, static_cast<int32_t>(1)));
                break;
            case Parameter::Acceleration:
                _seal_acceleration = static_cast<uint32_t>(
                    std::max(msg.value, static_cast<int32_t>(0)));
                break;
            case Parameter::StallguardThreshold: {
                static constexpr const int32_t min_sgt = -64;
                static constexpr const int32_t max_sgt = 63;
                auto value = std::clamp(msg.value, min_sgt, max_sgt);
                _tmc2130.get_register_map().coolconf.sgt = value;
                ret = _tmc2130.write_config(policy);
                break;
            }
            case Parameter::StallguardMinVelocity: {
                auto value =
                    motor_util::SealStepper::velocity_to_tstep(msg.value);
                static constexpr const uint32_t min_tstep = 0;
                static constexpr const uint32_t max_tstep = (1 << 20) - 1;
                value = std::clamp(static_cast<uint32_t>(value), min_tstep,
                                   max_tstep);
                _tmc2130.get_register_map().tcoolthrs.threshold = value;
                ret = _tmc2130.write_config(policy);
                break;
            }
            case Parameter::RunCurrent: {
                static constexpr const uint32_t min_current = 0;
                static constexpr const uint32_t max_current = 0x1F;
                auto value = std::clamp(static_cast<uint32_t>(msg.value),
                                        min_current, max_current);
                _tmc2130.get_register_map().ihold_irun.run_current = value;
                ret = _tmc2130.write_config(policy);
                break;
            }
            case Parameter::HoldCurrent: {
                static constexpr const uint32_t min_current = 0;
                static constexpr const uint32_t max_current = 0x1F;
                auto value = std::clamp(static_cast<uint32_t>(msg.value),
                                        min_current, max_current);
                _tmc2130.get_register_map().ihold_irun.hold_current = value;
                ret = _tmc2130.write_config(policy);
                break;
            }
        }

        if (!ret) {
            with_error = errors::ErrorCode::SEAL_MOTOR_SPI_ERROR;
        }

        auto response = messages::AcknowledgePrevious{
            .responding_to_id = msg.id, .with_error = with_error};
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::GetLidStatusMessage& msg, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto lid = motor_util::LidStepper::Status::UNKNOWN;
        auto seal = _seal_position;

        if (policy.lid_read_closed_switch()) {
            lid = motor_util::LidStepper::Status::CLOSED;
        } else if (policy.lid_read_open_switch()) {
            lid = motor_util::LidStepper::Status::OPEN;
        } else if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            lid = motor_util::LidStepper::Status::BETWEEN;
        }

        if (_seal_stepper_state.status != SealStepperState::Status::IDLE) {
            seal = motor_util::SealStepper::Status::BETWEEN;
        }

        auto response = messages::GetLidStatusResponse{
            .responding_to_id = msg.id, .lid = lid, .seal = seal};
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::OpenLidMessage& msg, Policy& policy)
        -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            response.with_error = errors::ErrorCode::LID_MOTOR_BUSY;
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
            return;
        }

        // First release the latch
        policy.lid_solenoid_engage();
        // Now start a lid motor movement to the endstop
        policy.lid_stepper_set_dac(LidStepperState::DEFAULT_RUN_CURRENT);
        policy.lid_stepper_start(LidStepperState::FULL_OPEN_DEGREES, false);
        // Store the new state, as well as the response ID
        _lid_stepper_state.status = LidStepperState::Status::OPEN_TO_SWITCH;
        _lid_stepper_state.response_id = msg.id;
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::CloseLidMessage& msg, Policy& policy)
        -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            response.with_error = errors::ErrorCode::LID_MOTOR_BUSY;
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
            return;
        }
        // First release the latch
        policy.lid_solenoid_engage();
        // Now start a lid motor movement to closed position
        policy.lid_stepper_set_dac(LidStepperState::DEFAULT_RUN_CURRENT);
        policy.lid_stepper_start(LidStepperState::FULL_CLOSE_DEGREES, false);
        // Store the new state, as well as the response ID
        _lid_stepper_state.status = LidStepperState::Status::CLOSE_TO_SWITCH;
        _lid_stepper_state.response_id = msg.id;
    }

    // Callback for each tick() during a seal stepper movement
    template <MotorExecutionPolicy Policy>
    auto seal_step_callback(Policy& policy) -> void {
        auto ret = _seal_profile.tick();
        if (ret.step) {
            policy.tmc2130_step_pulse();
        }
        if (ret.done) {
            policy.seal_stepper_stop();
            // Send a 'done' message to ourselves
            static_cast<void>(get_message_queue().try_send_from_isr(
                messages::SealStepperComplete{}));
        }
    }

    /**
     * @brief Contains all logic for starting a seal movement. If an ACK needs
     * to be sent after the movement, the caller should set the response_id
     * field.
     *
     * @param[in] steps Number of steps to move. This is \e signed, positive
     * values move forwards and negative values move backwards.
     * @param[in] policy Instance of the policy for motor control.
     */
    template <MotorExecutionPolicy Policy>
    auto start_seal_movement(long steps, Policy& policy) -> errors::ErrorCode {
        // Movement profile gets constructed with default parameters
        _seal_profile = motor_util::MovementProfile(
            policy.MotorTickFrequency, 0, _seal_velocity, _seal_acceleration,
            motor_util::MovementType::FixedDistance, std::abs(steps));

        // Steps is signed, so set direction accordingly
        auto ret = policy.tmc2130_set_direction(steps > 0);
        if (!ret) {
            return errors::ErrorCode::SEAL_MOTOR_FAULT;
        }

        ret = policy.tmc2130_set_enable(false);
        if (!ret) {
            return errors::ErrorCode::SEAL_MOTOR_FAULT;
        }

        auto err = clear_seal_stall(policy);
        if (err != errors::ErrorCode::NO_ERROR) {
            return err;
        }

        ret = policy.tmc2130_set_enable(true);
        if (!ret) {
            return errors::ErrorCode::SEAL_MOTOR_FAULT;
        }

        _seal_stepper_state.status = SealStepperState::Status::MOVING;

        ret = policy.seal_stepper_start(
            [&] { this->seal_step_callback(policy); });
        if (!ret) {
            _seal_stepper_state.status = SealStepperState::Status::IDLE;
            return errors::ErrorCode::SEAL_MOTOR_FAULT;
        }

        return errors::ErrorCode::NO_ERROR;
    }

    /**
     * @brief This function should clear the stall flag in the TMC2130.
     * Enables and then disables the StealthChop mode (which isn't used in
     * this application), which clears the data for StallGuard.
     * @param policy Instance of the policy for motor control
     * @return errors::ErrorCode
     */
    template <MotorExecutionPolicy Policy>
    auto clear_seal_stall(Policy& policy) -> errors::ErrorCode {
        auto tcool = _tmc2130.get_register_map().tcoolthrs.threshold;
        _tmc2130.get_register_map().gconfig.en_pwm_mode = 1;
        _tmc2130.get_register_map().tcoolthrs.threshold = 0;
        if (!_tmc2130.write_config(policy)) {
            return errors::ErrorCode::SEAL_MOTOR_SPI_ERROR;
        }
        _tmc2130.get_register_map().gconfig.en_pwm_mode = 0;
        _tmc2130.get_register_map().tcoolthrs.threshold = tcool;
        if (!_tmc2130.write_config(policy)) {
            return errors::ErrorCode::SEAL_MOTOR_SPI_ERROR;
        }

        return errors::ErrorCode::NO_ERROR;
    }

    /**
     * @brief Handler to transition between lid motor states. Should be
     * called every time a lid motor movement complete callback is triggered.
     * @param policy Instance of the policy for motor control
     * @return errors::ErrorCode
     */
    template <MotorExecutionPolicy Policy>
    auto handle_lid_state_end(Policy& policy) -> errors::ErrorCode {
        auto error = errors::ErrorCode::NO_ERROR;
        switch (_lid_stepper_state.status.load()) {
            case LidStepperState::Status::SIMPLE_MOVEMENT:
                // Turn off the drive current
                policy.lid_stepper_set_dac(0);
                // Movement is done
                _lid_stepper_state.status = LidStepperState::Status::IDLE;
                break;
            case LidStepperState::Status::OPEN_TO_SWITCH:
                // Now that the lid is at the open position,
                // the solenoid can be safely turned off
                policy.lid_solenoid_disengage();
                // Move to the Open Back To 90 step
                policy.lid_stepper_start(
                    LidStepperState::OPEN_BACK_TO_90_DEGREES, false);
                _lid_stepper_state.status =
                    LidStepperState::Status::OPEN_BACK_TO_90;
                break;
            case LidStepperState::Status::OPEN_BACK_TO_90:
                // Turn off lid stepper current
                policy.lid_stepper_set_dac(0);
                // Movement is done
                _lid_stepper_state.status = LidStepperState::Status::IDLE;
                break;
            case LidStepperState::Status::CLOSE_TO_SWITCH:
                // Overdrive the lid stepper into the switch
                policy.lid_stepper_start(
                    LidStepperState::CLOSE_OVERDRIVE_DEGREES, true);
                _lid_stepper_state.status =
                    LidStepperState::Status::CLOSE_OVERDRIVE;
                break;
            case LidStepperState::Status::CLOSE_OVERDRIVE:
                // Now that the lid is at the closed position,
                // the solenoid can be safely turned off
                policy.lid_solenoid_disengage();
                // Turn off lid stepper current
                policy.lid_stepper_set_dac(0);
                // Movement is done
                _lid_stepper_state.status = LidStepperState::Status::IDLE;
                // TODO(Frank, Mar-7-2022) check if the lid didn't make it in
                // all the way
                break;
            case LidStepperState::Status::IDLE:
                [[fallthrough]];
            default:
                break;
        }

        return error;
    }

    Queue& _message_queue;
    tasks::Tasks<QueueImpl>* _task_registry;
    LidStepperState _lid_stepper_state;
    SealStepperState _seal_stepper_state;
    tmc2130::TMC2130 _tmc2130;
    motor_util::MovementProfile _seal_profile;
    double _seal_velocity;
    double _seal_acceleration;
    /**
     * @brief  We need to cache the position of the seal motor in addition to
     * the state in _seal_stepper_state due to the lack of limit switches.
     *
     * The lid stepper has switches to tell where it is, so we don't
     * need a similar variable for that motor.
     */
    motor_util::SealStepper::Status _seal_position;
};

};  // namespace motor_task
