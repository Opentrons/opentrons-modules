/**
 * @file motor_task.hpp
 * @brief Primary interface for the motor task
 *
 */
#pragma once
#include <cmath>

#include "core/ack_cache.hpp"
#include "core/linear_motion_system.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/motor_interrupt.hpp"
#include "firmware/motor_policy.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "flex-stacker/tmc2160_registers.hpp"
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

struct MotorState {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    lms::LinearMotionSystemConfig lms_config;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float speed_mm_per_sec;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float accel_mm_per_sec_sq;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float speed_mm_per_sec_discont;
    [[nodiscard]] auto get_usteps_per_mm() const -> float {
        return lms_config.get_usteps_per_mm();
    }
    [[nodiscard]] auto get_speed() const -> float {
        return speed_mm_per_sec * get_usteps_per_mm();
    }
    [[nodiscard]] auto get_accel() const -> float {
        return accel_mm_per_sec_sq * get_usteps_per_mm();
    }
    [[nodiscard]] auto get_speed_discont() const -> float {
        return speed_mm_per_sec_discont * get_usteps_per_mm();
    }
    [[nodiscard]] auto get_distance(float mm) const -> float {
        return mm * get_usteps_per_mm();
    }
};

struct Defaults {
    struct X {
        static constexpr float SPEED = 200.0;
        static constexpr float ACCELERATION = 50.0;
        static constexpr float SPEED_DISCONT = 5.0;

        static constexpr float MM_PER_REV =
            lms::LeadScrewConfig::mm_per_rev(9.7536, 1.0);
        static constexpr float STEPS_PER_REV = 200;
        static constexpr float MICROSTEP = 16;
    };

    struct Z {
        static constexpr float SPEED = 200.0;
        static constexpr float ACCELERATION = 50.0;
        static constexpr float SPEED_DISCONT = 5.0;

        static constexpr float MM_PER_REV =
            lms::LeadScrewConfig::mm_per_rev(9.7536, 1.0);
        static constexpr float STEPS_PER_REV = 200;
        static constexpr float MICROSTEP = 16;
    };

    struct L {
        static constexpr float SPEED = 200.0;
        static constexpr float ACCELERATION = 50.0;
        static constexpr float SPEED_DISCONT = 5.0;

        static constexpr float MM_PER_REV =
            lms::GearBoxConfig::mm_per_rev(16.0, 16.0 / 30.0);
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

  public:
    explicit MotorTask(Queue& q, Aggregator* aggregator, Controller& x_ctrl,
                       Controller& z_ctrl, Controller& l_ctrl)
        : _message_queue(q),
          _task_registry(aggregator),
          _x_controller(x_ctrl),
          _z_controller(z_ctrl),
          _l_controller(l_ctrl),
          _initialized(false) {}
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
    template <MotorControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MotorEnableMessage& m, Policy& policy)
        -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        if (!(motor_enable(MotorID::MOTOR_X, m.x, policy) &&
              motor_enable(MotorID::MOTOR_Z, m.z, policy) &&
              motor_enable(MotorID::MOTOR_L, m.l, policy))) {
            response.with_error = m.x ? errors::ErrorCode::MOTOR_ENABLE_FAILED
                                      : errors::ErrorCode::MOTOR_DISABLE_FAILED;
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto motor_enable(MotorID id, std::optional<bool> engage, Policy& policy)
        -> bool {
        if (!engage.has_value()) {
            return true;
        }
        return engage.value() ? policy.enable_motor(id)
                              : policy.disable_motor(id);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveMotorInStepsMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        auto direction = m.steps > 0;
        controller_from_id(m.motor_id)
            .start_fixed_movement(m.id, direction, std::abs(m.steps), 0,
                                  m.steps_per_second, m.steps_per_second_sq);
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveMotorInMmMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto direction = m.mm > 0;
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
        controller_from_id(m.motor_id)
            .start_fixed_movement(m.id, direction,
                                  state.get_distance(std::abs(m.mm)),
                                  state.get_speed_discont(), state.get_speed(),
                                  state.get_accel());
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveToLimitSwitchMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
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
        controller_from_id(m.motor_id)
            .start_movement(m.id, m.direction, state.get_speed_discont(),
                            state.get_speed(), state.get_accel());
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::StopMotorMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
        controller_from_id(m.motor_id).stop_movement(m.id, true);
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
            .l_released_triggered =
                policy.check_limit_switch(MotorID::MOTOR_L, true),
            .l_held_triggered =
                policy.check_limit_switch(MotorID::MOTOR_L, false),
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GetPlatformSensorsMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        static_cast<void>(m);
//        auto response = messages::GetPlatformSensorsResponse{
//            .responding_to_id = m.id,
//            .extend =
//                policy.check_limit_switch(MotorID::MOTOR_X, true),
//            .retract =
//                policy.check_limit_switch(MotorID::MOTOR_X, false)
//        };
//        static_cast<void>(_task_registry->send_to_address(
//            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveCompleteMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto response = messages::AcknowledgePrevious{
            .responding_to_id =
                controller_from_id(m.motor_id).get_response_id()};
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::SetMicrostepsMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        // sent from the driver task so we know we've written to driver
        // successfully
        motor_state(m.motor_id).lms_config.microstep =
            pow(2, m.microsteps_power);
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GetMoveParamsMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        MotorState& state = motor_state(m.motor_id);
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

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GPIOInterruptMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
        _z_controller.stop_movement(0, true);
        _x_controller.stop_movement(0, false);
        _l_controller.stop_movement(0, false);
        auto msg = messages::ErrorMessage{
            .code = errors::ErrorCode::MOTOR_STALL_DETECTED};
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    Controller& _x_controller;
    Controller& _z_controller;
    Controller& _l_controller;
    bool _initialized;
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
};

};  // namespace motor_task
