#include "simulator/motor_thread.hpp"

#include <cstdint>
#include <memory>
#include <stop_token>
#include <thread>

#include "heater-shaker/errors.hpp"
#include "heater-shaker/motor_task.hpp"
#include "heater-shaker/tasks.hpp"

using namespace motor_thread;

struct SimMotorPolicy {
    static constexpr int32_t DEFAULT_RAMP_RATE_RPM_PER_S = 1000;
    static constexpr int32_t MAX_RAMP_RATE_RPM_PER_S = 20000;
    static constexpr int32_t MIN_RAMP_RATE_RPM_PER_S = 1;

    auto set_rpm(int16_t rpm) -> errors::ErrorCode {
        rpm_setpoint = rpm;
        rpm_current = rpm;
        return errors::ErrorCode::NO_ERROR;
    };
    [[nodiscard]] auto get_current_rpm() const -> int16_t {
        return rpm_current;
    }
    [[nodiscard]] auto get_target_rpm() const -> int16_t {
        return rpm_setpoint;
    }
    auto stop() -> void {
        rpm_setpoint = 0;
        rpm_current = 0;
    }

    auto set_ramp_rate(int32_t rpm_per_s) -> errors::ErrorCode {
        ramp_rate = rpm_per_s;
        return errors::ErrorCode::NO_ERROR;
    }

    auto homing_solenoid_disengage() const -> void {}

    auto homing_solenoid_engage(uint16_t current_ma) const -> void {
        static_cast<void>(current_ma);
    }

    auto delay_ticks(uint16_t ticks) -> void { static_cast<void>(ticks); }

  private:
    int16_t rpm_setpoint = 0;
    int16_t rpm_current = 0;
    int32_t ramp_rate = DEFAULT_RAMP_RATE_RPM_PER_S;
};

struct motor_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimMotorTask::Queue()), task(SimMotorTask(queue)) {}
    SimMotorTask::Queue queue;
    SimMotorTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    auto policy = SimMotorPolicy();
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
        } catch (const SimMotorTask::Queue::StopDuringMsgWait& sdmw) {
            return;
        }
        if (tcb->task.get_state() ==
            motor_task::State::HOMING_COASTING_TO_STOP) {
            static_cast<void>(tcb->queue.try_send(
                messages::MotorSystemErrorMessage{.errors = 2}));
        }
    }
}

auto motor_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimMotorTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task{std::make_unique<std::jthread>(run, tcb), &tcb->task};
}
