#include "simulator/motor_thread.hpp"

#include "simulator/sim_tmc2130_policy.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace motor_thread;

class SimMotorPolicy : public SimTMC2130Policy {
  public:
    using Callback = std::function<void()>;

    /** Frequency of the seal motor interrupt in hertz.*/
    static constexpr const uint32_t MotorTickFrequency = 1000000;

    SimMotorPolicy() : SimTMC2130Policy(), _callback() {}

    // Functionality to fulfill concept

    auto lid_stepper_set_dac(uint8_t dac_val) -> void { _dac_val = dac_val; }
    auto lid_stepper_start(int32_t steps, bool overdrive) -> void {
        _lid_overdrive = overdrive;
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

    auto lid_read_closed_switch() -> bool { return _lid_closed_switch; }

    auto lid_read_open_switch() -> bool { return _lid_open_switch; }

    auto seal_stepper_start(Callback cb) -> bool {
        if (_seal_moving) {
            return false;
        }

        _seal_moving = true;
        _callback = cb;
        return true;
    }

    auto seal_stepper_stop() -> void { _seal_moving = false; }

    // For simulation
    auto tick() -> void {
        if (_seal_moving) {
            _callback();
        }
    }

  private:
    // Solenoid is engaged when unpowered
    bool _solenoid_engaged = true;
    uint8_t _dac_val = 0;
    int32_t _actual_angle = 0;
    bool _moving = false;
    bool _lid_fault = false;
    bool _lid_open_switch = false;
    bool _lid_closed_switch = false;
    bool _seal_moving = false;
    bool _lid_overdrive = false;
    Callback _callback;
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
