/**
 * @file motor_task.hpp
 * @brief Primary interface for the motor task
 *
 */
#pragma once
#include <cmath>
#include <cstdint>

#include "core/ack_cache.hpp"
#include "core/circular_buffer.hpp"
#include "core/linear_motion_system.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/motor_interrupt.hpp"
#include "firmware/motor_policy.hpp"
#include "incubator-module/errors.hpp"
#include "incubator-module/messages.hpp"
#include "incubator-module/motor_utils.hpp"
#include "incubator-module/tasks.hpp"
#include "incubator-module/tmc5160_registers.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"

namespace motor_task {
template <typename P>
concept MotorControlPolicy = requires(P p, MotorID motor_id) {
    { p.enable_motor(motor_id) } -> std::same_as<bool>;
    { p.disable_motor(motor_id) } -> std::same_as<bool>;
};

using Message = messages::MotorMessage;
using Controller = motor_interrupt_controller::MotorInterruptController;
using Move = motor_interrupt_controller::Move;
using Error = errors::ErrorCode;

struct Defaults {
    struct R {
        static constexpr float SPEED = 100.0;
        static constexpr float ACCELERATION = 500.0;
        static constexpr float SPEED_DISCONT = 100.0;

        static constexpr float MM_PER_REV =
            lms::GearBoxConfig::mm_per_rev(30, 30.0 / 16.0);
        static constexpr float STEPS_PER_REV = 200;
        static constexpr float MICROSTEP = 16;
    };
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;
    static constexpr size_t MOVE_BUFFER_SIZE = 10;
    using MoveBuffer = circular_buffer::CircularBuffer<Move, MOVE_BUFFER_SIZE>;
    using MotorState = motor_util::MotorState;

  public:
    explicit MotorTask(Queue& q, Aggregator* aggregator, Controller& r_ctrl)
        : _message_queue(q),
          _task_registry(aggregator),
          _r_controller(r_ctrl) {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    auto controller_from_id(MotorID motor_id) -> Controller& {
        switch (motor_id) {
            case MotorID::MOTOR_R:
                return _r_controller;
            default:
                return _r_controller;
        }
    }

    auto motor_state(MotorID motor_id) -> MotorState& {
        switch (motor_id) {
            case MotorID::MOTOR_R:
                return _r_state;
            default:
                return _r_state;
        }
    }

    template <MotorControlPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        auto message = Message(std::monostate());

        if (!_initialized) {
            _r_controller.initialize(&policy);
            _message_queue.set_ready();
            _initialized = true;
        }

        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    auto all_motors_idle() -> Error {
        if (_r_controller.is_moving()) {
            return Error::R_MOTOR_BUSY;
        }
        return Error::NO_ERROR;
    }

    auto stop_motors(Error error) -> void {
        _r_controller.stop_movement(error, true);
        _move_queue.reset();
    }

    auto make_move(uint32_t id, MotorID motor_id, bool direction,
                   float distance, bool has_next_move = false) -> Move {
        return Move{.motor_id = motor_id,
                    .motor_state = &motor_state(motor_id),
                    .move_id = id,
                    .direction = direction,
                    .distance = distance,
                    .to_home_sensor = false,
                    .has_next_move = has_next_move};
    }

    auto make_home_move(uint32_t id, MotorID motor_id, bool direction,
                        bool has_next_move = false) -> Move {
        return Move{.motor_id = motor_id,
                    .motor_state = &motor_state(motor_id),
                    .move_id = id,
                    .direction = direction,
                    .distance = 0,
                    .to_home_sensor = true,
                    .has_next_move = has_next_move};
    }

    auto schedule_move(Move to_scheduled) -> void {
        auto scheduled = _move_queue.enqueue(to_scheduled);
        if (!scheduled) {
            send_error_message(Error::MOTOR_QUEUE_FULL);
        }
    }

    auto send_error_message(Error error) -> void {
        if (_task_registry) {
            auto msg = messages::ErrorMessage{.code = error};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    auto send_ack_message(uint32_t response_id, Error error = Error::NO_ERROR)
        -> void {
        if (_task_registry) {
            auto msg = messages::AcknowledgePrevious{
                .responding_to_id = response_id, .with_error = error};
            static_cast<void>(
                _task_registry->send_to_address(msg, Queues::HostCommsAddress));
        }
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MotorEnableMessage& m, Policy& policy)
        -> void {
        motor_enable(MotorID::MOTOR_R, m.r, policy);
        send_ack_message(m.id);
    }

    template <MotorControlPolicy Policy>
    auto motor_enable(MotorID id, std::optional<bool> engage, Policy& policy)
        -> void {
        if (!engage.has_value()) {
            return;
        }

        auto result =
            engage.value() ? policy.enable_motor(id) : policy.disable_motor(id);
        if (!result) {
            send_error_message(engage.value() ? Error::MOTOR_ENABLE_FAILED
                                              : Error::MOTOR_DISABLE_FAILED);
        }
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveMotorInStepsMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        auto error = all_motors_idle();
        if (error != Error::NO_ERROR) {
            send_ack_message(m.id, error);
            return;
        }
        auto direction = m.steps > 0;
        controller_from_id(m.motor_id)
            .start_fixed_movement(m.id, direction, std::abs(m.steps), 0,
                                  m.steps_per_second, m.steps_per_second_sq);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveMotorInMmMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto error = all_motors_idle();
        if (error != Error::NO_ERROR) {
            send_ack_message(m.id, error);
            return;
        }
        Controller& controller = controller_from_id(m.motor_id);
        MotorState& state = motor_state(m.motor_id);
        if (m.mm_per_second.has_value()) {
            state.speed_mm_per_sec = m.mm_per_second.value();
        }
        if (m.mm_per_second_sq.has_value()) {
            state.accel_mm_per_sec_sq = m.mm_per_second_sq.value();
        }
        if (m.mm_per_second_discont.has_value()) {
            state.speed_mm_per_sec_discont = m.mm_per_second_discont.value();
        }
        auto direction = m.mm > 0;
        auto move =
            make_move(m.id, m.motor_id, direction, std::abs(m.mm), false);
        controller.start_move(move);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::StopMotorMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        stop_motors(Error::STOP_REQUESTED);
        send_ack_message(m.id);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveCompleteMessage& m, Policy& policy)
        -> void {
        Move next_move;
        if (_move_queue.dequeue(next_move)) {
            // if there's a next move in the queue, start it
            controller_from_id(next_move.motor_id).start_move(next_move);
        } else {
            if (m.motor_id == MotorID::MOTOR_R) {
                policy.disable_motor(m.motor_id);
            }
            send_ack_message(controller_from_id(m.motor_id).get_response_id(),
                             controller_from_id(m.motor_id).get_error_code());
        }
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::SetMicrostepsMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        // sent from the driver task so we know we've written to driver
        // successfully
        motor_state(m.motor_id).lms_config.microstep =
            pow(2, m.microsteps_power);
        send_ack_message(m.id);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GetMoveParamsMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        const MotorState& state = motor_state(m.motor_id);
        auto response = messages::GetMoveParamsResponse{
            .responding_to_id = m.id,
            .motor_id = m.motor_id,
            .velocity = state.speed_mm_per_sec,
            .acceleration = state.accel_mm_per_sec_sq,
            .velocity_discont = state.speed_mm_per_sec_discont,
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::SetDiag0IRQMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        // NOTE: The diag0 pin is shared by all motors.
        _r_controller.set_diag0_irq(m.enable);
    }

    /**
     * @brief Move the motor in m.direction until the inductive home sensor
     * detects the target.
     */
    template <MotorControlPolicy Policy>
    auto visit_message(const messages::HomeMotorMessage& m, Policy& policy)
        -> void {
        auto error = all_motors_idle();
        if (error != Error::NO_ERROR) {
            send_ack_message(m.id, error);
            return;
        }
        if (policy.check_home_sensor(m.motor_id)) {
            send_ack_message(m.id);
            return;
        }
        _move_queue.reset();
        auto move = make_home_move(m.id, m.motor_id, m.direction);
        controller_from_id(m.motor_id).start_move(move);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    Controller& _r_controller;
    bool _initialized{false};

    MotorState _r_state{
        .lms_config = {.mm_per_rev = Defaults::R::MM_PER_REV,
                       .steps_per_rev = Defaults::R::STEPS_PER_REV,
                       .microstep = Defaults::R::MICROSTEP},
        .speed_mm_per_sec = Defaults::R::SPEED,
        .accel_mm_per_sec_sq = Defaults::R::ACCELERATION,
        .speed_mm_per_sec_discont = Defaults::R::SPEED_DISCONT,
    };
    MoveBuffer _move_queue;
};

}  // namespace motor_task
