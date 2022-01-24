#include "simulator/motor_thread.hpp"

#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace motor_thread;

class SimMotorPolicy {
  public:
    // Functionality to fulfill concept

    auto lid_stepper_set_dac(uint8_t dac_val) -> void { _dac_val = dac_val; }
    auto lid_stepper_start(int32_t steps) -> void {
        // Simulate jumping right to the end
        if (_lid_fault) {
            return;
        }
        _actual_angle += steps;
        _moving = false;
    }
    auto lid_stepper_stop() -> void { _moving = false; }

    auto lid_stepper_check_fault() -> bool { return _lid_fault; }

    auto lid_stepper_reset() -> bool {
        _moving = false;
        _dac_val = 0;
        _lid_fault = false;
        return true;
    }

    auto lid_solenoid_disengage() -> void { _solenoid_engaged = false; }
    auto lid_solenoid_engage() -> void { _solenoid_engaged = true; }

    // Test-specific functions

    auto solenoid_engaged() -> bool { return _solenoid_engaged; }

    auto trigger_lid_fault() -> void { _lid_fault = true; }

  private:
    // Solenoid is engaged when unpowered
    bool _solenoid_engaged = true;
    uint8_t _dac_val = 0;
    int32_t _actual_angle = 0;
    bool _moving = false;
    bool _lid_fault = false;
};

struct motor_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimMotorTask::Queue()), task(SimMotorTask(queue)) {}
    SimMotorTask::Queue queue;
    SimMotorTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimMotorPolicy();
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
        } catch (const SimMotorTask::Queue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto motor_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimMotorTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
