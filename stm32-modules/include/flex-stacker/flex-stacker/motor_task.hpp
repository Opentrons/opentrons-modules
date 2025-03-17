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
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/motor_utils.hpp"
#include "flex-stacker/tasks.hpp"
#include "flex-stacker/tmc2160_registers.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"

namespace motor_task {
using namespace ot_utils::freertos_timer;

template <typename P>
concept MotorControlPolicy = requires(P p, MotorID motor_id) {
    { p.enable_motor(motor_id) } -> std::same_as<bool>;
    { p.disable_motor(motor_id) } -> std::same_as<bool>;
};

using Message = messages::MotorMessage;
using Controller = motor_interrupt_controller::MotorInterruptController;
using Move = motor_interrupt_controller::Move;
using Error = errors::ErrorCode;

// Gpio irq debounce time
static constexpr uint32_t DEBOUNCE_MS = 1000;
static constexpr uint32_t DEBOUNCE_SLEEP_MS = 200;
static constexpr uint32_t LED_ERROR_DURATION_MS = 1000;
static constexpr uint32_t LED_ERROR_REPS = 3;

struct Defaults {
    struct X {
        static constexpr float SPEED = 200.0;
        static constexpr float ACCELERATION = 1500.0;
        static constexpr float SPEED_DISCONT = 40.0;

        static constexpr float MM_PER_REV =
            lms::LeadScrewConfig::mm_per_rev(9.7536, 1.0);
        static constexpr float STEPS_PER_REV = 200;
        static constexpr float MICROSTEP = 16;

        // switch-to-switch: 192.5 mm - 5.0 mm offset
        static constexpr float FAST_HOME_DISTANCE = 187.5;
    };

    struct Z {
        static constexpr float SPEED = 150.0;
        static constexpr float ACCELERATION = 500.0;
        static constexpr float SPEED_DISCONT = 25.0;

        static constexpr float MM_PER_REV =
            lms::LeadScrewConfig::mm_per_rev(9.7536, 1.0);
        static constexpr float STEPS_PER_REV = 200;
        static constexpr float MICROSTEP = 16;

        // switch-to-switch: 136.0 mm - 5.0 mm offset
        static constexpr float FAST_HOME_DISTANCE = 131.0;
    };

    struct L {
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
    explicit MotorTask(Queue& q, Aggregator* aggregator, Controller& x_ctrl,
                       Controller& z_ctrl, Controller& l_ctrl)
        : _message_queue(q),
          _task_registry(aggregator),
          _x_controller(x_ctrl),
          _z_controller(z_ctrl),
          _l_controller(l_ctrl),
          _debounce_timer(
              "DB Timer", [ThisPtr = this] { ThisPtr->reset_debounce(); },
              DEBOUNCE_MS) {}
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
            case MotorID::MOTOR_X:
                return _x_controller;
            case MotorID::MOTOR_Z:
                return _z_controller;
            case MotorID::MOTOR_L:
                return _l_controller;
            default:
                return _x_controller;
        }
    }

    auto motor_state(MotorID motor_id) -> MotorState& {
        switch (motor_id) {
            case MotorID::MOTOR_X:
                return _x_state;
            case MotorID::MOTOR_Z:
                return _z_state;
            case MotorID::MOTOR_L:
                return _l_state;
            default:
                return _x_state;
        }
    }

    template <MotorControlPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        auto message = Message(std::monostate());

        if (!_initialized) {
            _x_controller.initialize(&policy);
            _z_controller.initialize(&policy);
            _l_controller.initialize(&policy);
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
        if (_x_controller.is_moving()) {
            return Error::X_MOTOR_BUSY;
        }
        if (_z_controller.is_moving()) {
            return Error::Z_MOTOR_BUSY;
        }
        if (_l_controller.is_moving()) {
            return Error::L_MOTOR_BUSY;
        }
        return Error::NO_ERROR;
    }

    auto stop_motors(Error error) -> void {
        _z_controller.stop_movement(error, true);
        _x_controller.stop_movement(error, false);
        _l_controller.stop_movement(error, false);
        _move_queue.reset();
    }

    auto make_move(uint32_t id, MotorID motor_id, bool direction,
                   float distance, bool has_next_move = false) -> Move {
        return Move{.motor_id = motor_id,
                    .motor_state = &motor_state(motor_id),
                    .move_id = id,
                    .direction = direction,
                    .distance = distance,
                    .limit_switch = false,
                    .has_next_move = has_next_move};
    }

    auto make_home_move(uint32_t id, MotorID motor_id, bool direction,
                        bool has_next_move = false) -> Move {
        return Move{.motor_id = motor_id,
                    .motor_state = &motor_state(motor_id),
                    .move_id = id,
                    .direction = direction,
                    .distance = 0,
                    .limit_switch = true,
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
        motor_enable(MotorID::MOTOR_X, m.x, policy);
        motor_enable(MotorID::MOTOR_Z, m.z, policy);
        motor_enable(MotorID::MOTOR_L, m.l, policy);
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
    auto visit_message(const messages::MoveToLimitSwitchMessage& m,
                       Policy& policy) -> void {
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
        auto move = make_home_move(m.id, m.motor_id, m.direction);
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
    auto visit_message(const messages::GetLimitSwitchesMessage& m,
                       Policy& policy) -> void {
        auto response = messages::GetLimitSwitchesResponses{
            .responding_to_id = m.id,
            .x_extend_triggered =
                policy.check_limit_switch(MotorID::MOTOR_X, true),
            .x_retract_triggered =
                policy.check_limit_switch(MotorID::MOTOR_X, false),
            .z_extend_triggered =
                policy.check_limit_switch(MotorID::MOTOR_Z, true),
            .z_retract_triggered =
                policy.check_limit_switch(MotorID::MOTOR_Z, false),
            .l_held_triggered =
                policy.check_limit_switch(MotorID::MOTOR_L, false),
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GetPlatformSensorsMessage& m,
                       Policy& policy) -> void {
        auto response = messages::GetPlatformSensorsResponse{
            .responding_to_id = m.id,
            .extend_presence = policy.check_platform_sensor(true),
            .retract_presence = policy.check_platform_sensor(false)};
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GetEstopMessage& m, Policy& policy)
        -> void {
        auto response = messages::GetEstopResponse{
            .responding_to_id = m.id, .triggered = policy.check_estop()};
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveCompleteMessage& m, Policy& policy)
        -> void {
        Move next_move;
        if (_move_queue.dequeue(next_move)) {
            // if there's a next move in the queue, start it
            controller_from_id(next_move.motor_id).start_move(next_move);
        } else {
            if (m.motor_id == MotorID::MOTOR_Z) {
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
        _x_controller.set_diag0_irq(m.enable);
    }

    auto reset_debounce() -> void { _debounce_timer.stop(); }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GPIOInterruptMessage& m, Policy& policy)
        -> void {
        // Debounce
        if (_debounce_timer.is_running()) {
            return;
        }
        _debounce_timer.start();

        auto triggered = false;
        if (policy.is_diag0_pin(m.pin)) {
            policy.sleep_ms(DEBOUNCE_SLEEP_MS);
            triggered = policy.check_diag0();
        } else if (policy.is_estop_pin(m.pin)) {
            policy.sleep_ms(DEBOUNCE_SLEEP_MS);
            triggered = policy.check_estop();
        } else {
            // don't care about other interrupts
            return;
        }

        // Set status bars
        auto color = triggered ? StatusBarColor::Red : StatusBarColor::Green;
        auto pattern =
            triggered ? StatusBarPattern::Flash : StatusBarPattern::Static;
        auto message = messages::SetStatusBarStateMessage{
            .color = color,
            .pattern = pattern,
            .duration = LED_ERROR_DURATION_MS,
            .reps = LED_ERROR_REPS};
        static_cast<void>(
            _task_registry->send_to_address(message, Queues::UIAddress));
    }

    /**
     * @brief Move the motor to the limit switch; apply fast moves to XZ motors
     * whenever possible.
     */
    template <MotorControlPolicy Policy>
    auto visit_message(const messages::HomeMotorMessage& m, Policy& policy)
        -> void {
        auto error = all_motors_idle();
        if (error != Error::NO_ERROR) {
            send_ack_message(m.id, error);
            return;
        }
        if (policy.check_limit_switch(m.motor_id, m.direction)) {
            // motor is already homed
            send_ack_message(m.id);
            return;
        }
        _move_queue.reset();
        Move move;
        if (m.motor_id != MotorID::MOTOR_L &&
            policy.check_limit_switch(m.motor_id, !m.direction)) {
            // if the opposite limit switch is triggered, we know where we are
            // and can do a fast homing routine
            schedule_move(make_home_move(m.id, m.motor_id, m.direction));
            auto distance = m.motor_id == MotorID::MOTOR_X
                                ? Defaults::X::FAST_HOME_DISTANCE
                                : Defaults::Z::FAST_HOME_DISTANCE;
            move = make_move(m.id, m.motor_id, m.direction, distance, true);
        } else {
            // we don't know where we are, move towards the limit switch slowly
            move = make_home_move(m.id, m.motor_id, m.direction);
        }
        controller_from_id(m.motor_id).start_move(move);
    }

    // TODO: FOR TESTING, this will move to tof_sensor_task
    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GetTOFSensorStatusMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    Controller& _x_controller;
    Controller& _z_controller;
    Controller& _l_controller;
    bool _initialized{false};
    FreeRTOSTimer _debounce_timer;

    MotorState _x_state{
        .lms_config = {.mm_per_rev = Defaults::X::MM_PER_REV,
                       .steps_per_rev = Defaults::X::STEPS_PER_REV,
                       .microstep = Defaults::X::MICROSTEP},
        .speed_mm_per_sec = Defaults::X::SPEED,
        .accel_mm_per_sec_sq = Defaults::X::ACCELERATION,
        .speed_mm_per_sec_discont = Defaults::X::SPEED_DISCONT,
    };
    MotorState _z_state{
        .lms_config = {.mm_per_rev = Defaults::Z::MM_PER_REV,
                       .steps_per_rev = Defaults::Z::STEPS_PER_REV,
                       .microstep = Defaults::Z::MICROSTEP},
        .speed_mm_per_sec = Defaults::Z::SPEED,
        .accel_mm_per_sec_sq = Defaults::Z::ACCELERATION,
        .speed_mm_per_sec_discont = Defaults::Z::SPEED_DISCONT,
    };
    MotorState _l_state{
        .lms_config = {.mm_per_rev = Defaults::L::MM_PER_REV,
                       .steps_per_rev = Defaults::L::STEPS_PER_REV,
                       .microstep = Defaults::L::MICROSTEP},
        .speed_mm_per_sec = Defaults::L::SPEED,
        .accel_mm_per_sec_sq = Defaults::L::ACCELERATION,
        .speed_mm_per_sec_discont = Defaults::L::SPEED_DISCONT,
    };
    MoveBuffer _move_queue;
};

}  // namespace motor_task
