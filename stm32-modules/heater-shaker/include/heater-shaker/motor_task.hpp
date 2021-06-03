/*
 * the primary interface to the motor control task
 */
#pragma once

#include <concepts>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
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
concept MotorExecutionPolicy = requires(Policy& p, const Policy& cp) {
    { p.set_rpm(static_cast<int16_t>(16)) }
    ->std::same_as<errors::ErrorCode>;
    { cp.get_current_rpm() }
    ->std::same_as<int16_t>;
    { cp.get_target_rpm() }
    ->std::same_as<int16_t>;
    {p.stop()};
    { p.set_ramp_rate(static_cast<int32_t>(8)) }
    ->std::same_as<errors::ErrorCode>;
    {p.homing_solenoid_disengage()};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.homing_solenoid_engage(122)};
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.delay_ticks(10)};
};

struct State {
    enum TaskStatus {
        STOPPED_UNKNOWN,  // Stopped but unclear whether we're homed (how we
                          // boot)
        RUNNING,          // Running under a speed control or ramping (including
                          // speed=0)
        ERROR,            // In an error state from the motor driver
        HOMING_MOVING_TO_HOME_SPEED,  // Heading towards an appropriate speed
                                      // for homing
        HOMING_COASTING_TO_STOP,  // solenoid engaged, waiting for it to fall
                                  // home
        STOPPED_HOMED             // Stopped and definitely homed
    };
    TaskStatus status;
};

constexpr size_t RESPONSE_LENGTH = 128;
using Message = ::messages::MotorMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message> class MotorTask {
    static constexpr const uint32_t HOMING_INTERSTATE_WAIT_TICKS = 100;

  public:
    static constexpr uint16_t HOMING_ROTATION_LIMIT_HIGH_RPM = 500;
    static constexpr uint16_t HOMING_ROTATION_LIMIT_LOW_RPM = 250;
    static constexpr uint16_t HOMING_ROTATION_LOW_MARGIN = 50;
    static constexpr uint16_t HOMING_SOLENOID_CURRENT_INITIAL = 300;
    static constexpr uint16_t HOMING_SOLENOID_CURRENT_HOLD = 75;
    static constexpr uint16_t HOMING_COAST_ROTATION = 2;
    using Queue = QueueImpl<Message>;
    explicit MotorTask(Queue& q)
        : state{.status = State::STOPPED_UNKNOWN},
          message_queue(q),
          task_registry(nullptr) {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    [[nodiscard]] auto get_state() const -> State::TaskStatus {
        return state.status;
    }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy> auto run_once(Policy& policy)
        -> void {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.

        static_cast<void>(message_queue.recv(&message));
        std::visit(
            [this, &policy](const auto& msg) -> void {
                this->visit_message(msg, policy);
            },
            message);
    }

  private:
    template <typename Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(_ignore);
        static_cast<void>(policy);
    }

    template <typename Policy>
    auto visit_message(const messages::SetRPMMessage& msg, Policy& policy)
        -> void {
        auto error = policy.set_rpm(msg.target_rpm);
        state.status = State::RUNNING;
        auto response = messages::AcknowledgePrevious{
            .responding_to_id = msg.id, .with_error = error};
        if (msg.from_system) {
            static_cast<void>(
                task_registry->system->get_message_queue().try_send(
                    messages::SystemMessage(response)));

        } else {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    auto visit_message(const messages::SetAccelerationMessage& msg,
                       Policy& policy) -> void {
        auto error = policy.set_ramp_rate(msg.rpm_per_s);
        auto response = messages::AcknowledgePrevious{
            .responding_to_id = msg.id, .with_error = error};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::GetRPMMessage& msg, Policy& policy)
        -> void {
        auto response =
            messages::GetRPMResponse{.responding_to_id = msg.id,
                                     .current_rpm = policy.get_current_rpm(),
                                     .setpoint_rpm = policy.get_target_rpm()};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::MotorSystemErrorMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        if (msg.errors == 0) {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(messages::ErrorMessage{
                        .code = errors::ErrorCode::MOTOR_SPURIOUS_ERROR})));
            return;
        }
        if (state.status == State::HOMING_COASTING_TO_STOP) {
            policy.homing_solenoid_engage(HOMING_SOLENOID_CURRENT_HOLD);
            policy.stop();
            state.status = State::STOPPED_HOMED;
        } else {
            for (auto offset = static_cast<uint8_t>(
                     errors::MotorErrorOffset::FOC_DURATION);
                 offset <=
                 static_cast<uint8_t>(errors::MotorErrorOffset::SW_ERROR);
                 offset++) {
                auto code = errors::from_motor_error(
                    msg.errors, static_cast<errors::MotorErrorOffset>(offset));
                if (code != errors::ErrorCode::NO_ERROR) {
                    state.status = State::ERROR;
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::HostCommsMessage(
                                messages::ErrorMessage{.code = code})));
                }
            }
        }
    }

    template <typename Policy>
    auto visit_message(const messages::CheckHomingStatusMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(msg);
        if (state.status == State::HOMING_MOVING_TO_HOME_SPEED) {
            if (policy.get_current_rpm() < HOMING_ROTATION_LIMIT_HIGH_RPM &&
                policy.get_current_rpm() > HOMING_ROTATION_LIMIT_LOW_RPM) {
                policy.homing_solenoid_engage(HOMING_SOLENOID_CURRENT_INITIAL);
                state.status = State::HOMING_COASTING_TO_STOP;
            } else {
                policy.delay_ticks(HOMING_INTERSTATE_WAIT_TICKS);
                static_cast<void>(get_message_queue().try_send(
                    messages::CheckHomingStatusMessage{}));
            }
        }
    }

    template <typename Policy>
    auto visit_message(const messages::BeginHomingMessage& msg, Policy& policy)
        -> void {
        static_cast<void>(msg);
        state.status = State::HOMING_MOVING_TO_HOME_SPEED;
        policy.homing_solenoid_disengage();
        policy.set_rpm(HOMING_ROTATION_LIMIT_LOW_RPM +
                       HOMING_ROTATION_LOW_MARGIN);
        policy.delay_ticks(HOMING_INTERSTATE_WAIT_TICKS);
        static_cast<void>(
            get_message_queue().try_send(messages::CheckHomingStatusMessage{}));
    }

    template <typename Policy>
    auto visit_message(const messages::ActuateSolenoidMessage& msg,
                       Policy& policy) -> void {
        state.status = State::STOPPED_UNKNOWN;
        if (msg.current_ma == 0) {
            policy.homing_solenoid_disengage();
        } else {
            policy.homing_solenoid_engage(static_cast<double>(msg.current_ma));
        }
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }
    State state;
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
};

};  // namespace motor_task
