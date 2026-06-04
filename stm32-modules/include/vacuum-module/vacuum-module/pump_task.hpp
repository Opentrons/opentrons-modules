#pragma once
#include <cmath>
#include <cstdint>

#include "core/ack_cache.hpp"
#include "core/pid.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "firmware/pump_policy.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "slew_rate_limiter.hpp"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace pump_task {

// The frequency the pump control loop runs at.
static constexpr const uint32_t CONTROL_PERIOD_HZ = 100;
static constexpr const uint32_t CONTROL_PERIOD_MS =
    (1.0F / CONTROL_PERIOD_HZ * 1000);
static constexpr const double RPM_SAMPLE_TIME_S = CONTROL_PERIOD_MS / 1000.0F;
static constexpr const double MS_TO_SECONDS = 0.001F;
static constexpr const double K_FF = MAX_PWM / MAX_RPM;
static constexpr const double PUMP_STOP_RPM_THRESH = 500;
static constexpr const float MIN_RAMP_RATE = 1;      // rpm/s
static constexpr const float DEFAULT_RAMP_RATE = 5;  // rpm/s
static constexpr const float MAX_RAMP_RATE = 20;     // rpm/s
static constexpr const int8_t MAX_PWM_JUMP = 1;      // pwm/tick

struct PumpControl {
    SlewRateLimiter slew;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    PID pid;  // RPM PID loop

    double target_rpm = 0;
    double current_rpm = 0;
    uint8_t current_pwm = 0;
    uint8_t target_pwm = 0;
    bool manual_control = false;

    uint32_t last_tick = 0;
    bool enable_pump = false;
    bool pump_running = false;
};

// Feed forward (K_FF) provides most of the power, so kp and ki can be small.
const PumpControl pump_control = {
    .pid = PID(0.1000F,            // kp
               0.0001F,            // ki
               0.0F,               // kd
               RPM_SAMPLE_TIME_S,  // sampletime
               MAX_PWM,            // windup_limit_high
               0),                 // windup_limit_low
};

template <typename P>
concept PumpControlPolicy = requires(P p) {
    {p.sleep_ms(1)};
};

using PumpPolicy = pump_policy::PumpPolicy;
using Message = messages::PumpMessage;
using Error = errors::ErrorCode;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class PumpTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit PumpTask(Queue& q, Aggregator* aggregator, PumpPolicy* policy)
        : _message_queue(q), _task_registry(aggregator), _policy(policy) {}
    PumpTask(const PumpTask& other) = delete;
    auto operator=(const PumpTask& other) -> PumpTask& = delete;
    PumpTask(PumpTask&& other) noexcept = delete;
    auto operator=(PumpTask&& other) noexcept -> PumpTask& = delete;
    ~PumpTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <PumpControlPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        if (!_initialized) {
            // Configure
            _pump_control.slew.configure(0, DEFAULT_RAMP_RATE);

            _policy = &policy;
            _message_queue.set_ready();
            _initialized = true;
        }

        auto message = Message(std::monostate());
        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
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

    template <PumpControlPolicy Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <PumpControlPolicy Policy>
    auto visit_message(const messages::PumpControlMessage& m, Policy& policy)
        -> void {
        static_cast<void>(m);
        // Get delta time
        auto timestamp = policy.get_time_ms();
        auto last_tick =
            _pump_control.last_tick ? _pump_control.last_tick : timestamp;
        auto delta_s = (timestamp - last_tick) * MS_TO_SECONDS;
        _pump_control.last_tick = last_tick;

        auto rpm = policy.get_pump_rpm();
        // Stop pump control
        if (rpm < PUMP_STOP_RPM_THRESH && !_pump_control.enable_pump) {
            policy.enable_pump_control(false);
            policy.set_pump_duty_cycle(0);
            policy.enable_pump_tach(false);
            policy.stop_pump_motor();

            _pump_control.pid.reset();
            _pump_control.slew.reset();

            _pump_control.pump_running = false;
            _pump_control.current_rpm = 0;
            _pump_control.current_pwm = 0;
            _pump_control.last_tick = 0;
            return;
        }

        // Compute the new duty cycle
        auto target_setpoint = _pump_control.target_rpm;
        target_setpoint = _pump_control.slew.update(target_setpoint, delta_s);
        auto difference = target_setpoint - rpm;
        auto pwm = _pump_control.pid.compute(difference, delta_s);
        // add feed-forward
        pwm = (target_setpoint * K_FF) + pwm;

        // Sudden, large pwm changes (e.g., 0% to 60% or 80% to 10%) can induce
        // significant back-EMF and result in large current and voltage spikes
        // which can shut down the system. Limit how fast the duty cycle changes
        // so the motor does not freak out.
        auto current_pwm = _pump_control.current_pwm;
        auto desired_pwm = _pump_control.enable_pump ? pwm : MIN_PWM;
        auto max_pwm_jump = _pump_control.enable_pump ? MAX_PWM_JUMP : 1;
        desired_pwm = std::clamp<uint8_t>(desired_pwm, MIN_PWM, MAX_PWM);
        if (desired_pwm > current_pwm + max_pwm_jump) {
            current_pwm += max_pwm_jump;
        } else if (desired_pwm < current_pwm - max_pwm_jump) {
            current_pwm -= max_pwm_jump;
        } else {
            current_pwm = desired_pwm;
        }

        // Manual override + safety clamp
        auto target_pwm = _pump_control.target_pwm;
        auto duty = target_pwm > 0 ? target_pwm : current_pwm;
        duty = std::clamp<uint8_t>(duty, 0, MAX_PWM);

        _pump_control.current_pwm = duty;
        _pump_control.current_rpm = rpm;
        policy.set_pump_duty_cycle(duty);
    }

    template <PumpControlPolicy Policy>
    auto visit_message(const messages::SetPumpStateMessage& m, Policy& policy)
        -> void {
        _pump_control.target_pwm =
            std::clamp<uint8_t>(m.duty_cycle, 0, MAX_PWM);
        _pump_control.target_rpm =
            std::clamp<double>(m.rpm_setpoint, 0, MAX_RPM);
        _pump_control.enable_pump = m.run_pump;
        _pump_control.manual_control = m.from_host;

        if (!_pump_control.pump_running) {
            policy.enable_pump_tach(true);
            policy.start_pump_motor();
            policy.enable_pump_control(true);
            _pump_control.pump_running = true;
        }

        if (m.from_host) {
            // Send notification to PressureTask so it can track vacuum duration
            // and perform waste detection using its sensor access and logic.
            // PressureTask will run monitoring but must not send control msgs
            // (SetPumpState) back in this mode.
            auto notify =
                messages::NotifyPumpRunMessage{.run_pump = m.run_pump,
                                               .pressure_percent = m.duty_cycle,
                                               .duration_s = m.duration_s,
                                               .timeout_s = m.timeout_s,
                                               .ramp_rate = m.ramp_rate,
                                               .vent_after = m.vent_after};
            static_cast<void>(_task_registry->send_to_address(
                notify, Queues::PressureAddress));
        }
        send_ack_message(m.id);
    }

    template <PumpControlPolicy Policy>
    auto visit_message(const messages::GetPumpStateMessage& m, Policy& policy)
        -> void {
        static_cast<void>(policy);
        auto msg = messages::GetPumpStateResponseMessage{
            .responding_to_id = m.id,
            .target_rpm = _pump_control.target_rpm,
            .current_rpm = _pump_control.current_rpm,
            .target_pwm = _pump_control.target_pwm,
            .current_pwm = _pump_control.current_pwm,
            .pump_running = _pump_control.pump_running,
            .manual_control = _pump_control.manual_control,
        };
        static_cast<void>(
            _task_registry->send_to_address(msg, Queues::HostCommsAddress));
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    PumpPolicy* _policy;
    bool _initialized{false};

    PumpControl _pump_control = pump_control;
};

}  // namespace pump_task
