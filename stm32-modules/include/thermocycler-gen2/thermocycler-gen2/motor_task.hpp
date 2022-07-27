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
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/motor_utils.hpp"
#include "thermocycler-gen2/tasks.hpp"
#include "thermocycler-gen2/tmc2130.hpp"

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
    // A function to arm the seal stepper extension limit switch
    {p.seal_switch_set_extension_armed()};
    // A function to arm the seal stepper retraction limit switch
    {p.seal_switch_set_retraction_armed()};
    // A function to disarm the seal stepper limit switch
    {p.seal_switch_set_disarmed()};
    // A function to read the seal extension limit switch
    { p.seal_read_extension_switch() } -> std::same_as<bool>;
    // A function to read the seal retraction limit switch
    { p.seal_read_retraction_switch() } -> std::same_as<bool>;
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
    // After opening to the open switch, the lid must re-close a few
    // degrees to be at exactly 90º
    constexpr static double OPEN_OVERDRIVE_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(-5);
    // Full open/close movements run until they hit an endstop switch, so the
    // distance is 120 degrees which is far wider than the actual travel angle.
    constexpr static double FULL_CLOSE_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(-120);
    // After closing to the switch, the lid must "overdrive" a few degrees to
    // be fully seated in the closed position. The endstop switch is ignored
    // for this movement.
    constexpr static double CLOSE_OVERDRIVE_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(-5);
    constexpr static double PLATE_LIFT_RAISE_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(20);
    constexpr static double PLATE_LIFT_LOWER_DEGREES =
        motor_util::LidStepper::angle_to_microsteps(-30);
    // States for lid stepper
    enum Status {
        IDLE,            /**< Not moving.*/
        SIMPLE_MOVEMENT, /**< Single stage movement.*/
        OPEN_TO_SWITCH,  /**< Open until the open switch is hit.*/
        OPEN_OVERDRIVE,  /**< Close from switch back to 90º position.*/
        CLOSE_TO_SWITCH, /**< Close lid until it hits the switch.*/
        CLOSE_OVERDRIVE, /**< Close lid a few degrees into the switch.*/
        LIFT_RAISE,      /**< Open lid to raise the plate lift.*/
        LIFT_LOWER,      /**< Close lid to lower the plate lift.*/
    };
    // Current status of the lid stepper. Declared atomic because
    // this flag is set & cleared by both the actual task context
    // and an interrupt context when the motor interrupt fires.
    std::atomic<Status> status;
    // Current position of the lid stepper. Only relevant if the
    // status is IDLE
    motor_util::LidStepper::Position position;
    // When a movement is complete, respond to this ID. Only relevant for
    // a simple Hinge movement.
    uint32_t response_id;
};

// Structure to encapsulate state of the seal stepper
struct SealStepperState {
    // Distance to fully extend the seal
    constexpr static signed long FULL_EXTEND_MICROSTEPS = -1750000;
    // Distance to slightly extend the seal before retracting from
    // an unknown state
    constexpr static signed long SHORT_EXTEND_MICROSTEPS = -100000;
    // Distance to fully retract the seal. This is as long as
    // the full extension, plus some spare distance to ensure a stall.
    constexpr static signed long FULL_RETRACT_MICROSTEPS =
        (FULL_EXTEND_MICROSTEPS * -1);
    // Distance to back off after triggering a limit switch
    constexpr static double SWITCH_BACKOFF_MM = 1.0F;
    // Distance to RETRACT to back off a limit switch
    constexpr static signed long SWITCH_BACKOFF_MICROSTEPS_RETRACT =
        motor_util::SealStepper::mm_to_steps(SWITCH_BACKOFF_MM);
    // Distance to EXTEND to back off a limit switch
    constexpr static signed long SWITCH_BACKOFF_MICROSTEPS_EXTEND =
        motor_util::SealStepper::mm_to_steps(SWITCH_BACKOFF_MM) * -1;
    // Run current value, approximately 825 mA
    constexpr static int DEFAULT_RUN_CURRENT = 15;
    // Default velocity for the seal stepper, in steps/second
    constexpr static double DEFAULT_VELOCITY = 200000;
    // Default acceleration for the seal stepper, in steps/second^2
    constexpr static double DEFAULT_ACCEL = 50000;
    // Default value of the Stallguard Threshold
    constexpr static signed int DEFAULT_STALLGUARD_THRESHOLD = 4;
    // Default minimum velocity for stallguard activation, as a tstep value
    constexpr static uint32_t DEFAULT_SG_MIN_VELOCITY =
        motor_util::SealStepper::velocity_to_tstep(60000);
    // Stallguard min velocity value that will fully disable stallguard,
    // as a tstep value
    constexpr static uint32_t DISABLED_SG_MIN_VELOCITY = 0;
    // Enumeration of legal stepper actions
    enum class Status { IDLE, MOVING };
    // Current status of the seal stepper. Declared atomic because
    // this flag is set & cleared by both the actual task context
    // and an interrupt context when the motor interrupt fires.
    std::atomic<Status> status;
    // When a movement is complete, respond to this ID. Only relevant for
    // a simple Seal movement.
    uint32_t response_id;
    // Direction of the current movement, since the steps stored in
    // the movement profile are unsigned
    bool direction;
};

// Structure to encapsulate state of the overall lid system
struct LidState {
    // Lid action state machine. Individual hinge/seal motor actions are
    // handled in their sub-state machines
    enum class Status {
        IDLE,                         /**< No lid action.*/
        OPENING_RETRACT_SEAL,         /**< Retracting seal before opening lid.*/
        OPENING_RETRACT_SEAL_BACKOFF, /**< Extend seal to ease off of the
                                           limit switch.*/
        OPENING_OPEN_HINGE,           /**< Opening lid hinge.*/
        CLOSING_RETRACT_SEAL,         /**< Retracting seal before closing lid.*/
        CLOSING_RETRACT_SEAL_BACKOFF, /**< Extend seal to ease off of the
                                           limit switch.*/
        CLOSING_CLOSE_HINGE,          /**< Closing lid hinge.*/
        CLOSING_EXTEND_SEAL,          /**< Extending seal after closing
                                           lid hinge.*/
        CLOSING_EXTEND_SEAL_BACKOFF,  /**< Retract seal to ease off of the
                                           limit switch.*/
        PLATE_LIFTING, /**< Lid is walking through its state machine.*/
    };
    // Current status of the lid. Declared atomic because
    // this flag is set & cleared by both the actual task context
    // and an interrupt context when the motor interrupt fires.
    std::atomic<Status> status;
    // When the full action is complete, respond to this ID
    uint32_t response_id;
};

static constexpr tmc2130::TMC2130RegisterMap default_tmc_config = {
    .gconfig = {.diag0_error = 1, .diag1_stall = 1},
    .ihold_irun = {.hold_current = 0x1,  // Approx 118mA
                   .run_current = SealStepperState::DEFAULT_RUN_CURRENT,
                   .hold_current_delay = 0b0111},
    .tpowerdown = {},
    .tcoolthrs = {.threshold = SealStepperState::DISABLED_SG_MIN_VELOCITY},
    .thigh = {.threshold = 0xFFFFF},
    .chopconf = {.toff = 0b101, .hstrt = 0b101, .hend = 0b11, .tbl = 0b10},
    .coolconf = {.sgt = SealStepperState::DEFAULT_STALLGUARD_THRESHOLD}};

using Message = ::messages::MotorMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
  public:
    using Queue = QueueImpl<Message>;

    // Default current to set for the lid stepper, in milliamperes
    static constexpr double LID_STEPPER_RUN_CURRENT =
        motor_util::LidStepper::current_to_dac(1200);
    // Default current to set for lid stepper for holding, in milliamperes
    static constexpr double LID_STEPPER_HOLD_CURRENT =
        motor_util::LidStepper::current_to_dac(300);
    // ID value to indicate that no response is actually needed from a
    // motor completion
    static constexpr uint32_t INVALID_ID = 0;

    explicit MotorTask(Queue& q)
        : _message_queue(q),
          _initialized(false),
          _task_registry(nullptr),
          _state{.status = LidState::Status::IDLE, .response_id = INVALID_ID},
          _lid_stepper_state{
              .status = LidStepperState::Status::IDLE,
              .position = motor_util::LidStepper::Position::BETWEEN,
              .response_id = INVALID_ID},
          _seal_stepper_state{.status = SealStepperState::Status::IDLE,
                              .response_id = INVALID_ID,
                              .direction = true},
          _tmc2130(default_tmc_config),
          // Seal movement profile is populated with mostly dummy values.
          // It is set before every movement so these are irrelevant.
          _seal_profile(1, 0, SealStepperState::DEFAULT_VELOCITY,
                        SealStepperState::DEFAULT_ACCEL,
                        motor_util::MovementType::OpenLoop, 0),
          _seal_velocity(SealStepperState::DEFAULT_VELOCITY),
          _seal_acceleration(SealStepperState::DEFAULT_ACCEL),
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

        if (!_initialized) {
            _initialized = true;
            _tmc2130.write_config(policy);
            policy.lid_stepper_set_dac(LID_STEPPER_HOLD_CURRENT);
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

    // Primarily for test integration, do not use for interprocess logic!!!
    [[nodiscard]] auto get_lid_state() const -> LidState::Status {
        return _state.status;
    }

    [[nodiscard]] auto get_seal_position() const
        -> motor_util::SealStepper::Status {
        if (_seal_stepper_state.status != SealStepperState::Status::IDLE) {
            return motor_util::SealStepper::Status::BETWEEN;
        }
        return _seal_position;
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
            policy.lid_stepper_set_dac(LID_STEPPER_RUN_CURRENT);
            policy.lid_stepper_start(
                motor_util::LidStepper::angle_to_microsteps(msg.angle),
                msg.overdrive);
            _lid_stepper_state.status =
                LidStepperState::Status::SIMPLE_MOVEMENT;
            _lid_stepper_state.position =
                motor_util::LidStepper::Position::BETWEEN;
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
        auto error = handle_hinge_state_end(policy);
        if (_lid_stepper_state.status == LidStepperState::Status::IDLE &&
            old_state != _lid_stepper_state.status &&
            _lid_stepper_state.response_id != INVALID_ID) {
            // Send an ACK if a movement just finished
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = _lid_stepper_state.response_id,
                .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
            _lid_stepper_state.response_id = INVALID_ID;
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
            error = start_seal_movement(msg.steps, true, policy);
        }

        // Check for error after starting movement
        if (error != errors::ErrorCode::NO_ERROR) {
            auto response =
                messages::SealStepperDebugResponse{.responding_to_id = msg.id,
                                                   .steps_taken = 0,
                                                   .with_error = error};
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
                    // Don't send an error because a stall is expected in some
                    // conditions. The number of steps will tell whether this
                    // stall was too early or not.
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
            if (with_error == errors::ErrorCode::NO_ERROR) {
                with_error = handle_lid_state_end(policy);
            } else {
                // Send error response on behalf of the lid state machine
                lid_response_send_and_clear(with_error);
                handle_lid_state_enter(LidState::Status::IDLE, policy);
            }
            if (_seal_stepper_state.response_id != INVALID_ID) {
                auto response = messages::SealStepperDebugResponse{
                    .responding_to_id = _seal_stepper_state.response_id,
                    .steps_taken =
                        static_cast<long int>(_seal_profile.current_distance()),
                    .with_error = with_error};
                if (!_seal_stepper_state.direction) {
                    response.steps_taken *= -1;
                }
                static_cast<void>(
                    _task_registry->comms->get_message_queue().try_send(
                        messages::HostCommsMessage(response)));
                _seal_stepper_state.response_id = INVALID_ID;
            }
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
        auto response =
            messages::GetSealDriveStatusResponse{.responding_to_id = msg.id};
        auto status = _tmc2130.get_driver_status(policy);
        if (status.has_value()) {
            response.status = status.value();
        }
        auto tstep = _tmc2130.get_tstep(policy);
        if (tstep.has_value()) {
            response.tstep = tstep.value();
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
        auto lid = get_lid_position(policy);
        auto seal = _seal_position;

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
        auto error = start_lid_open(msg.id, policy);

        if (error != errors::ErrorCode::NO_ERROR) {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = msg.id, .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::CloseLidMessage& msg, Policy& policy)
        -> void {
        auto error = start_lid_close(msg.id, policy);

        if (error != errors::ErrorCode::NO_ERROR) {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = msg.id, .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::PlateLiftMessage& msg, Policy& policy)
        -> void {
        auto error = start_plate_lift(msg.id, policy);

        if (error != errors::ErrorCode::NO_ERROR) {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = msg.id, .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::FrontButtonPressMessage& msg,
                       Policy& policy) {
        static_cast<void>(msg);
        if (is_any_motor_moving()) {
            // Ignore button press during any lid movement.
            return;
        }

        auto lid_position = get_lid_position(policy);
        if (lid_position == motor_util::LidStepper::Position::UNKNOWN) {
            return;
        }
        if(msg.long_press) {
            // Long presses are for plate lift
            if (lid_position == motor_util::LidStepper::Position::OPEN) {
                static_cast<void>(start_plate_lift(INVALID_ID, policy));
            }
        } else {
            // Short presses are for opening/closing the lid
            if (lid_position == motor_util::LidStepper::Position::OPEN) {
                static_cast<void>(start_lid_close(INVALID_ID, policy));
            } else {
                // Default to opening the lid if the status is in-between switches
                static_cast<void>(start_lid_open(INVALID_ID, policy));
            }
        }
    }

    template <MotorExecutionPolicy Policy>
    auto visit_message(const messages::GetLidSwitchesMessage& msg,
                       Policy& policy) {
        auto response = messages::GetLidSwitchesResponse{
            .responding_to_id = msg.id,
            .close_switch_pressed = policy.lid_read_closed_switch(),
            .open_switch_pressed = policy.lid_read_open_switch(),
            .seal_extension_pressed = policy.seal_read_extension_switch(),
            .seal_retraction_pressed = policy.seal_read_retraction_switch()};

        static_cast<void>(
            _task_registry->comms->get_message_queue().try_send(response));
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
    auto start_seal_movement(long steps, bool arm_limit_switch, Policy& policy)
        -> errors::ErrorCode {
        if (_seal_stepper_state.status != SealStepperState::Status::IDLE) {
            return errors::ErrorCode::SEAL_MOTOR_BUSY;
        }

        // Movement profile gets constructed with default parameters
        _seal_profile = motor_util::MovementProfile(
            policy.MotorTickFrequency, 0, _seal_velocity, _seal_acceleration,
            motor_util::MovementType::FixedDistance, std::abs(steps));

        _seal_stepper_state.direction = steps > 0;

        // This disarms both switches, and is performed before EACH movement
        // to prevent two consecutive movements from enabling both switches
        policy.seal_switch_set_disarmed();

        if (arm_limit_switch) {
            if (_seal_stepper_state.direction) {
                // Positive numbers are for retraction

                // If we are moving until a seal limit switch event, it is
                // important that the switch is NOT already triggered.
                if (policy.seal_read_retraction_switch()) {
                    return errors::ErrorCode::SEAL_MOTOR_SWITCH;
                }
                policy.seal_switch_set_retraction_armed();
            } else {
                // Negative numbers are for extension

                // If we are moving until a seal limit switch event, it is
                // important that the switch is NOT already triggered.
                if (policy.seal_read_extension_switch()) {
                    return errors::ErrorCode::SEAL_MOTOR_SWITCH;
                }
                policy.seal_switch_set_extension_armed();
            }
        }

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
        _seal_position = motor_util::SealStepper::Status::UNKNOWN;

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

    template <MotorExecutionPolicy Policy>
    [[nodiscard]] auto get_lid_position(Policy& policy) const
        -> motor_util::LidStepper::Position {
        auto closed_switch = policy.lid_read_closed_switch();
        auto open_switch = policy.lid_read_open_switch();

        if (_state.status != LidState::Status::IDLE) {
            // ALWAYS return Between during a movement
            return motor_util::LidStepper::Position::BETWEEN;
        }
        if (closed_switch && open_switch) {
            return motor_util::LidStepper::Position::UNKNOWN;
        }
        if (closed_switch) {
            return motor_util::LidStepper::Position::CLOSED;
        }
        if (open_switch) {
            return motor_util::LidStepper::Position::OPEN;
        }
        if (!is_any_motor_moving()) {
            // Wait until both motors are COMPLETELY stopped to report
            // the actual position
            return _lid_stepper_state.position;
        }
        return motor_util::LidStepper::Position::BETWEEN;
    }

    [[nodiscard]] auto is_any_motor_moving() const -> bool {
        if (_seal_stepper_state.status != SealStepperState::Status::IDLE) {
            return true;
        }
        if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            return true;
        }
        if (_state.status != LidState::Status::IDLE) {
            return true;
        }
        return false;
    }

    /**
     * @brief Start an action to open the lid assembly. This will retract the
     * seal (if necessary) and then open the lid hinge motor.
     *
     * @tparam Policy Type of the motor policy
     * @param response_id The message ID to respond to at the end of the
     *                    action. This will \e only be cached if the lid
     *                    can succesfully start opening.
     * @param policy Instance of the HAL policy
     * @return errorcode indicating success or failure of the action
     */
    template <MotorExecutionPolicy Policy>
    auto start_lid_open(uint32_t response_id, Policy& policy)
        -> errors::ErrorCode {
        if (is_any_motor_moving()) {
            return errors::ErrorCode::LID_MOTOR_BUSY;
        }
        auto error = errors::ErrorCode::NO_ERROR;
        if (get_lid_position(policy) ==
            motor_util::LidStepper::Position::OPEN) {
            // Send a succesful response and return ok
            auto response =
                messages::AcknowledgePrevious{.responding_to_id = response_id};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
            return error;
        }
        // Always retract the seal before opening
        error = handle_lid_state_enter(LidState::Status::OPENING_RETRACT_SEAL,
                                       policy);

        if (error == errors::ErrorCode::NO_ERROR) {
            _state.response_id = response_id;
        }
        return error;
    }

    /**
     * @brief Start an action to close the lid assembly. This will retract the
     * seal (if necessary), close the lid hinge motor, and then extend the
     * seal. If the lid is already shut with the seal extended, nothing will
     * happen but a response message \e will be sent.
     *
     * @tparam Policy Type of the motor policy
     * @param response_id The message ID to respond to at the end of the
     *                    action. This will \e only be cached if the lid
     *                    can succesfully start opening.
     * @param policy Instance of the HAL policy
     * @return errorcode indicating success or failure of the action
     */
    template <MotorExecutionPolicy Policy>
    auto start_lid_close(uint32_t response_id, Policy& policy)
        -> errors::ErrorCode {
        if (is_any_motor_moving()) {
            return errors::ErrorCode::LID_MOTOR_BUSY;
        }
        auto error = errors::ErrorCode::NO_ERROR;
        if (get_lid_position(policy) ==
            motor_util::LidStepper::Position::CLOSED) {
            // Send a succesful response and return ok
            auto response =
                messages::AcknowledgePrevious{.responding_to_id = response_id};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
            return error;
        }

        // Always retract seal before closing
        error = handle_lid_state_enter(LidState::Status::CLOSING_RETRACT_SEAL,
                                       policy);

        if (error == errors::ErrorCode::NO_ERROR) {
            _state.response_id = response_id;
        }
        return error;
    }

    /**
     * @brief Start a Plate Lift action. This action can only be started if
     * the lid is at the Open position. The lid will open \e past the endstop
     * switch in order to lift the plate, and then will return back to the
     * 90º position at the switch.
     *
     * @tparam Policy Type of the motor policy
     * @param response_id The message ID to respond to at the end of the
     *                    action. This will \e only be cached if the lid
     *                    can succesfully start lifting.
     * @param policy Instance of the HAL policy
     * @return errorcode indicating success or failure of the action
     */
    template <MotorExecutionPolicy Policy>
    auto start_plate_lift(uint32_t response_id, Policy& policy)
        -> errors::ErrorCode {
        if (is_any_motor_moving()) {
            return errors::ErrorCode::LID_MOTOR_BUSY;
        }
        if (get_lid_position(policy) !=
            motor_util::LidStepper::Position::OPEN) {
            return errors::ErrorCode::LID_CLOSED;
        }
        auto error =
            handle_lid_state_enter(LidState::Status::PLATE_LIFTING, policy);

        if (error == errors::ErrorCode::NO_ERROR) {
            _state.response_id = response_id;
        }
        return error;
    }

    template <MotorExecutionPolicy Policy>
    auto start_lid_hinge_open(uint32_t response_id, Policy& policy) -> bool {
        if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            return false;
        }
        // First release the latch
        policy.lid_solenoid_engage();
        // Now start a lid motor movement to the endstop
        policy.lid_stepper_set_dac(LID_STEPPER_RUN_CURRENT);
        policy.lid_stepper_start(LidStepperState::FULL_OPEN_DEGREES, false);
        // Store the new state, as well as the response ID
        _lid_stepper_state.status = LidStepperState::Status::OPEN_TO_SWITCH;
        _lid_stepper_state.position = motor_util::LidStepper::Position::BETWEEN;
        _lid_stepper_state.response_id = response_id;
        return true;
    }

    template <MotorExecutionPolicy Policy>
    auto start_lid_hinge_close(uint32_t response_id, Policy& policy) -> bool {
        if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            return false;
        }
        // First release the latch
        policy.lid_solenoid_engage();
        // Now start a lid motor movement to closed position
        policy.lid_stepper_set_dac(LID_STEPPER_RUN_CURRENT);
        policy.lid_stepper_start(LidStepperState::FULL_CLOSE_DEGREES, false);
        // Store the new state, as well as the response ID
        _lid_stepper_state.status = LidStepperState::Status::CLOSE_TO_SWITCH;
        _lid_stepper_state.position = motor_util::LidStepper::Position::BETWEEN;
        _lid_stepper_state.response_id = response_id;
        return true;
    }

    template <MotorExecutionPolicy Policy>
    auto start_lid_hinge_plate_lift(uint32_t response_id, Policy& policy)
        -> bool {
        if (_lid_stepper_state.status != LidStepperState::Status::IDLE) {
            return false;
        }
        // Now start a lid motor movement to closed position
        policy.lid_stepper_set_dac(LID_STEPPER_RUN_CURRENT);
        policy.lid_stepper_start(LidStepperState::PLATE_LIFT_RAISE_DEGREES,
                                 true);
        // Store the new state, as well as the response ID
        _lid_stepper_state.status = LidStepperState::Status::LIFT_RAISE;
        _lid_stepper_state.position = motor_util::LidStepper::Position::BETWEEN;
        _lid_stepper_state.response_id = response_id;
        return true;
    }

    /**
     * @brief If the the lid state machine has a response code defined, send
     * the response to host and then clear the error code.
     *
     * @param error Optional error to attach to the response
     */
    auto lid_response_send_and_clear(
        errors::ErrorCode error = errors::ErrorCode::NO_ERROR) -> void {
        if (_state.response_id != INVALID_ID) {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = _state.response_id, .with_error = error};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
        _state.response_id = INVALID_ID;
    }

    /**
     * @brief Handler to enter lid states. Generally called to start
     * the lid state machine, and then subsequently by `handle_lid_state_end`
     *
     * @param policy
     * @return errors::ErrorCode
     */
    template <MotorExecutionPolicy Policy>
    auto handle_lid_state_enter(LidState::Status state, Policy& policy)
        -> errors::ErrorCode {
        auto error = errors::ErrorCode::NO_ERROR;
        auto state_for_system_task =
            messages::UpdateMotorState::MotorState::IDLE;
        switch (state) {
            case LidState::Status::IDLE:
                lid_response_send_and_clear();
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::IDLE;
                break;
            case LidState::Status::OPENING_RETRACT_SEAL:
                // The seal stepper is retracted to the limit switch
                error = start_seal_movement(
                    SealStepperState::FULL_RETRACT_MICROSTEPS, true, policy);
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::OPENING_RETRACT_SEAL_BACKOFF:
                // The seal stepper is extended to back off the limit switch
                error = start_seal_movement(
                    SealStepperState::SWITCH_BACKOFF_MICROSTEPS_EXTEND, false,
                    policy);
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::OPENING_OPEN_HINGE:
                if (!start_lid_hinge_open(INVALID_ID, policy)) {
                    error = errors::ErrorCode::LID_MOTOR_BUSY;
                }
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::CLOSING_RETRACT_SEAL:
                // The seal stepper is retracted to a stall
                error = start_seal_movement(
                    SealStepperState::FULL_RETRACT_MICROSTEPS, true, policy);
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::CLOSING_RETRACT_SEAL_BACKOFF:
                // The seal stepper is extended to back off the limit switch
                error = start_seal_movement(
                    SealStepperState::SWITCH_BACKOFF_MICROSTEPS_EXTEND, false,
                    policy);
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::CLOSING_CLOSE_HINGE:
                if (!start_lid_hinge_close(INVALID_ID, policy)) {
                    error = errors::ErrorCode::LID_MOTOR_BUSY;
                }
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::CLOSING_EXTEND_SEAL:
                // The seal stepper is extended to engage with the plate
                error = start_seal_movement(
                    SealStepperState::FULL_EXTEND_MICROSTEPS, true, policy);
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::CLOSING_EXTEND_SEAL_BACKOFF:
                // The seal stepper is extended to back off the limit switch
                error = start_seal_movement(
                    SealStepperState::SWITCH_BACKOFF_MICROSTEPS_RETRACT, false,
                    policy);
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::OPENING_OR_CLOSING;
                break;
            case LidState::Status::PLATE_LIFTING:
                // The lid state machine handles everything
                if (!start_lid_hinge_plate_lift(INVALID_ID, policy)) {
                    error = errors::ErrorCode::LID_MOTOR_FAULT;
                }
                state_for_system_task =
                    messages::UpdateMotorState::MotorState::PLATE_LIFT;
                break;
        }
        if (error == errors::ErrorCode::NO_ERROR) {
            _state.status = state;
        } else {
            _state.status = LidState::Status::IDLE;
            state_for_system_task =
                messages::UpdateMotorState::MotorState::IDLE;
        }
        static_cast<void>(_task_registry->system->get_message_queue().try_send(
            messages::UpdateMotorState{.state = state_for_system_task}));
        return error;
    }

    /**
     * @brief Handler to end lid states. This state machine refers to the
     * \e entire lid assembly - both the hinge motor and the seal motor.
     * @details
     * In general, this function should be called by the handlers for the
     * Seal and Hinge state machines once either of them finishes a movement.
     * This state machine covers the overall behavior of the lid, and each
     * step of that process involves one of the two motors moving at a time.
     * @param policy Instance of the policy for motor control
     * @return errors::ErrorCode
     */
    template <MotorExecutionPolicy Policy>
    auto handle_lid_state_end(Policy& policy) -> errors::ErrorCode {
        auto error = errors::ErrorCode::NO_ERROR;
        switch (_state.status) {
            case LidState::Status::IDLE:
                // Do nothing
                break;
            case LidState::Status::OPENING_RETRACT_SEAL:
                _seal_position = motor_util::SealStepper::Status::BETWEEN;
                // Start lid motor movement
                error = handle_lid_state_enter(
                    LidState::Status::OPENING_RETRACT_SEAL_BACKOFF, policy);
                break;
            case LidState::Status::OPENING_RETRACT_SEAL_BACKOFF:
                _seal_position = motor_util::SealStepper::Status::RETRACTED;
                // Start lid motor movement
                error = handle_lid_state_enter(
                    LidState::Status::OPENING_OPEN_HINGE, policy);
                break;
            case LidState::Status::OPENING_OPEN_HINGE:
                error = handle_lid_state_enter(LidState::Status::IDLE, policy);
                break;
            case LidState::Status::CLOSING_RETRACT_SEAL:
                _seal_position = motor_util::SealStepper::Status::BETWEEN;
                error = handle_lid_state_enter(
                    LidState::Status::CLOSING_RETRACT_SEAL_BACKOFF, policy);
                break;
            case LidState::Status::CLOSING_RETRACT_SEAL_BACKOFF:
                _seal_position = motor_util::SealStepper::Status::RETRACTED;
                // Start lid motor movement
                error = handle_lid_state_enter(
                    LidState::Status::CLOSING_CLOSE_HINGE, policy);
                break;
            case LidState::Status::CLOSING_CLOSE_HINGE:
                error = handle_lid_state_enter(
                    LidState::Status::CLOSING_EXTEND_SEAL, policy);
                break;
            case LidState::Status::CLOSING_EXTEND_SEAL:
                _seal_position = motor_util::SealStepper::Status::BETWEEN;
                error = handle_lid_state_enter(
                    LidState::Status::CLOSING_EXTEND_SEAL_BACKOFF, policy);
                break;
            case LidState::Status::CLOSING_EXTEND_SEAL_BACKOFF:
                _seal_position = motor_util::SealStepper::Status::ENGAGED;
                // Start lid motor movement
                error = handle_lid_state_enter(LidState::Status::IDLE, policy);
                break;
            case LidState::Status::PLATE_LIFTING:
                error = handle_lid_state_enter(LidState::Status::IDLE, policy);
                break;
        }
        if (error != errors::ErrorCode::NO_ERROR) {
            // Clear the lid status no matter what
            lid_response_send_and_clear(error);
            handle_lid_state_enter(LidState::Status::IDLE, policy);
        }
        return error;
    }

    /**
     * @brief Handler to transition between lid hinge motor states. Should be
     * called every time a lid motor movement complete callback is triggered.
     * @param policy Instance of the policy for motor control
     * @return errors::ErrorCode
     */
    template <MotorExecutionPolicy Policy>
    auto handle_hinge_state_end(Policy& policy) -> errors::ErrorCode {
        auto error = errors::ErrorCode::NO_ERROR;
        switch (_lid_stepper_state.status.load()) {
            case LidStepperState::Status::SIMPLE_MOVEMENT:
                // Turn off the drive current
                policy.lid_stepper_set_dac(LID_STEPPER_HOLD_CURRENT);
                // Movement is done
                _lid_stepper_state.status = LidStepperState::Status::IDLE;
                _lid_stepper_state.position =
                    motor_util::LidStepper::Position::BETWEEN;
                break;
            case LidStepperState::Status::OPEN_TO_SWITCH:
                // Now that the lid is at the open position,
                // the solenoid can be safely turned off
                policy.lid_solenoid_disengage();
                // Overdrive into switch
                policy.lid_stepper_start(
                    LidStepperState::OPEN_OVERDRIVE_DEGREES, true);
                _lid_stepper_state.status =
                    LidStepperState::Status::OPEN_OVERDRIVE;
                break;
            case LidStepperState::Status::OPEN_OVERDRIVE:
                // Turn off lid stepper current
                policy.lid_stepper_set_dac(LID_STEPPER_HOLD_CURRENT);
                // Movement is done
                _lid_stepper_state.status = LidStepperState::Status::IDLE;
                _lid_stepper_state.position =
                    motor_util::LidStepper::Position::OPEN;
                // The overall lid state machine can advance now
                error = handle_lid_state_end(policy);
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
                policy.lid_stepper_set_dac(LID_STEPPER_HOLD_CURRENT);
                // Movement is done
                _lid_stepper_state.status = LidStepperState::Status::IDLE;
                _lid_stepper_state.position =
                    motor_util::LidStepper::Position::CLOSED;
                // The overall lid state machine can advance now
                error = handle_lid_state_end(policy);
                // TODO(Frank, Mar-7-2022) check if the lid didn't make it in
                // all the way
                break;
            case LidStepperState::Status::LIFT_RAISE:
                // Lower the plate lift mechanism and move the lid far enough
                // that it will go PAST the switch.
                policy.lid_stepper_start(
                    LidStepperState::PLATE_LIFT_LOWER_DEGREES, true);
                _lid_stepper_state.status = LidStepperState::Status::LIFT_LOWER;
                break;
            case LidStepperState::Status::LIFT_LOWER:
                // We switch to the Open To Switch state, which will get the
                // lid to the 90º position.
                policy.lid_stepper_start(LidStepperState::FULL_OPEN_DEGREES,
                                         false);
                _lid_stepper_state.status =
                    LidStepperState::Status::OPEN_TO_SWITCH;
                break;
            case LidStepperState::Status::IDLE:
                [[fallthrough]];
            default:
                break;
        }

        return error;
    }

    Queue& _message_queue;
    bool _initialized;
    tasks::Tasks<QueueImpl>* _task_registry;
    LidState _state;
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
