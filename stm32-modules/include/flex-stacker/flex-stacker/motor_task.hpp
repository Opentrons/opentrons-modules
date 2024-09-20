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

namespace motor_task {

template <typename P>
concept MotorControlPolicy = requires(P p, MotorID motor_id) {
    { p.enable_motor(motor_id) } -> std::same_as<bool>;
    { p.disable_motor(motor_id) } -> std::same_as<bool>;
};

using Message = messages::MotorMessage;
using Controller = motor_interrupt_controller::MotorInterruptController;

static constexpr struct lms::LinearMotionSystemConfig<lms::LeadScrewConfig>
    motor_x_config = {
    .mech_config = lms::LeadScrewConfig{.lead_screw_pitch = 9.7536,
                                        .gear_reduction_ratio = 1.0},
    .steps_per_rev = 200, .microstep = 16,
};
static constexpr struct lms::LinearMotionSystemConfig<lms::LeadScrewConfig>
    motor_z_config = {
    .mech_config = lms::LeadScrewConfig{.lead_screw_pitch = 9.7536,
                                        .gear_reduction_ratio = 1.0},
    .steps_per_rev = 200, .microstep = 16,
};
static constexpr struct lms::LinearMotionSystemConfig<lms::GearBoxConfig>
    motor_l_config = {
    .mech_config = lms::GearBoxConfig{.gear_diameter = 16.0,
                                      .gear_reduction_ratio = 16.0 / 30.0},
    .steps_per_rev = 200, .microstep = 16,
};

struct MotorState {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float steps_per_mm;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float speed_mm_per_sec;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float accel_mm_per_sec_sq;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    float speed_mm_per_sec_discont;
    [[nodiscard]] auto get_speed() const -> float {
        return speed_mm_per_sec * steps_per_mm;
    }
    [[nodiscard]] auto get_accel() const -> float {
        return accel_mm_per_sec_sq * steps_per_mm * steps_per_mm;
    }
    [[nodiscard]] auto get_speed_discont() const -> float {
        return speed_mm_per_sec_discont * steps_per_mm;
    }
    [[nodiscard]] auto get_distance(float mm) const -> float {
        return mm * steps_per_mm;
    }
};

struct XState {
    static constexpr float DEFAULT_SPEED = 200.0;
    static constexpr float DEFAULT_ACCELERATION = 50.0;
    static constexpr float DEFAULT_SPEED_DISCONT = 5.0;
};

struct ZState {
    static constexpr float DEFAULT_SPEED = 200.0;
    static constexpr float DEFAULT_ACCELERATION = 50.0;
    static constexpr float DEFAULT_SPEED_DISCONT = 5.0;
};
struct LState {
    static constexpr float DEFAULT_SPEED = 200.0;
    static constexpr float DEFAULT_ACCELERATION = 50.0;
    static constexpr float DEFAULT_SPEED_DISCONT = 5.0;
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
        if (m.mm_per_second.has_value()) {
            motor_state(m.motor_id).speed_mm_per_sec = m.mm_per_second.value();
        }
        if (m.mm_per_second_sq.has_value()) {
            motor_state(m.motor_id).accel_mm_per_sec_sq =
                m.mm_per_second_sq.value();
        }
        if (m.mm_per_second_discont.has_value()) {
            motor_state(m.motor_id).speed_mm_per_sec_discont =
                m.mm_per_second_discont.value();
        }
        controller_from_id(m.motor_id)
            .start_fixed_movement(
                m.id, direction,
                motor_state(m.motor_id).get_distance(std::abs(m.mm)),
                motor_state(m.motor_id).get_speed_discont(),
                motor_state(m.motor_id).get_speed(),
                motor_state(m.motor_id).get_accel());

        auto stream_m = messages::PollTMCRegisterMessage{
            .id = m.id,
            .motor_id = m.motor_id,
        };
        static_cast<void>(
            _task_registry->send_to_address(stream_m, Queues::MotorDriverAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::MoveToLimitSwitchMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        if (m.mm_per_second.has_value()) {
            motor_state(m.motor_id).speed_mm_per_sec = m.mm_per_second.value();
        }
        if (m.mm_per_second_sq.has_value()) {
            motor_state(m.motor_id).accel_mm_per_sec_sq =
                m.mm_per_second_sq.value();
        }
        if (m.mm_per_second_discont.has_value()) {
            motor_state(m.motor_id).speed_mm_per_sec_discont =
                m.mm_per_second_discont.value();
        }
        controller_from_id(m.motor_id)
            .start_movement(m.id, m.direction,
                            motor_state(m.motor_id).get_speed_discont(),
                            motor_state(m.motor_id).get_speed(),
                            motor_state(m.motor_id).get_accel());
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::StopMotorMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
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
    auto visit_message(const messages::MoveCompleteMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        static_cast<void>(_task_registry->send_to_address(
            messages::StopPollTMCRegisterMessage{}, Queues::MotorDriverAddress));
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
        switch (m.motor_id) {
            case MotorID::MOTOR_X:
                _x_mech_conf.microstep =
                    static_cast<float>(pow(2, m.microsteps_power));
                _x_state.steps_per_mm = _x_mech_conf.get_usteps_per_mm();
                break;
            case MotorID::MOTOR_Z:
                _z_mech_conf.microstep =
                    static_cast<float>(pow(2, m.microsteps_power));
                _z_state.steps_per_mm = _z_mech_conf.get_usteps_per_mm();
                break;
            case MotorID::MOTOR_L:
                _l_mech_conf.microstep =
                    static_cast<float>(pow(2, m.microsteps_power));
                _l_state.steps_per_mm = _l_mech_conf.get_usteps_per_mm();
                break;
            default:
                break;
        }
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <MotorControlPolicy Policy>
    auto visit_message(const messages::GetMoveParamsMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto response = messages::GetMoveParamsResponse{
            .responding_to_id = m.id,
            .motor_id = m.motor_id,
            .velocity = motor_state(m.motor_id).speed_mm_per_sec,
            .acceleration = motor_state(m.motor_id).accel_mm_per_sec_sq,
            .velocity_discont =
                motor_state(m.motor_id).speed_mm_per_sec_discont,
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    Controller& _x_controller;
    Controller& _z_controller;
    Controller& _l_controller;
    bool _initialized;
    lms::LinearMotionSystemConfig<lms::LeadScrewConfig> _x_mech_conf =
        motor_x_config;
    lms::LinearMotionSystemConfig<lms::LeadScrewConfig> _z_mech_conf =
        motor_z_config;
    lms::LinearMotionSystemConfig<lms::GearBoxConfig> _l_mech_conf =
        motor_l_config;
    MotorState _x_state{
        .steps_per_mm = motor_x_config.get_usteps_per_mm(),
        .speed_mm_per_sec = XState::DEFAULT_SPEED,
        .accel_mm_per_sec_sq = XState::DEFAULT_SPEED,
        .speed_mm_per_sec_discont = XState::DEFAULT_SPEED_DISCONT,
    };
    MotorState _z_state{
        .steps_per_mm = motor_z_config.get_usteps_per_mm(),
        .speed_mm_per_sec = XState::DEFAULT_SPEED,
        .accel_mm_per_sec_sq = XState::DEFAULT_SPEED,
        .speed_mm_per_sec_discont = XState::DEFAULT_SPEED_DISCONT,
    };
    MotorState _l_state{
        .steps_per_mm = motor_l_config.get_usteps_per_mm(),
        .speed_mm_per_sec = LState::DEFAULT_SPEED,
        .accel_mm_per_sec_sq = LState::DEFAULT_SPEED,
        .speed_mm_per_sec_discont = LState::DEFAULT_SPEED_DISCONT,
    };
};

};  // namespace motor_task
