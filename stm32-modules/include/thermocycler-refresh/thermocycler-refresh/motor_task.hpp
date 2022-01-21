/*
 * the primary interface to the motor control task
 */
#pragma once

#include <algorithm>
#include <concepts>
#include <variant>

#include "hal/message_queue.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/tasks.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic pop

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
    // A function to set the stepper drive current in mV
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.lid_stepper_set_vref(1)};
    // A function to start a stepper movement
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.lid_stepper_start(1.0F)};
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
};

struct LidStepperState {
    enum LidStepperTaskStatus { IDLE = 0, MOVING = 1 };
    // Current status of the lid stepper
    LidStepperTaskStatus status;
    // When a movement is complete, respond to this ID
    uint32_t response_id;
};

using Message = ::messages::MotorMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
  public:
    using Queue = QueueImpl<Message>;
    explicit MotorTask(Queue& q)
        : message_queue(q),
          task_registry(nullptr),
          lid_stepper_state{.status = LidStepperState::IDLE} {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

    template <typename Policy>
    requires MotorExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
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
    auto visit_message(const messages::LidStepperDebugMessage& msg,
                       Policy& policy) -> void {
        // check for errors
        policy.lid_stepper_set_vref(48);  // test
        policy.lid_stepper_start(msg.angle);
        lid_stepper_state.status = LidStepperState::MOVING;
        lid_stepper_state.response_id = msg.id;
    }

    template <typename Policy>
    auto visit_message(const messages::LidStepperComplete& msg, Policy& policy)
        -> void {
        if(lid_stepper_state.status == LidStepperState::MOVING) {
            lid_stepper_state.status = LidStepperState::IDLE;
            policy.lid_stepper_set_vref(0);
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = lid_stepper_state.response_id};
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));    
        }
    }

    template <typename Policy>
    auto visit_message(const messages::ActuateSolenoidMessage& msg,
                       Policy& policy) -> void {
        if (msg.engage) {
            policy.lid_solenoid_engage();
        } else {
            policy.lid_solenoid_disengage();
        }
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    LidStepperState lid_stepper_state;
};

};  // namespace motor_task
