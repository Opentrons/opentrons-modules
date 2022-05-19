#include "simulator/motor_thread.hpp"

#include <algorithm>

#include "simulator/sim_tmc2130_policy.hpp"
#include "systemwide.h"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/motor_utils.hpp"
#include "thermocycler-gen2/tasks.hpp"

using namespace motor_thread;

class SimMotorPolicy : public SimTMC2130Policy {
  public:
    using Callback = std::function<void()>;

    /** Frequency of the seal motor interrupt in hertz.*/
    static constexpr const uint32_t MotorTickFrequency = 1000000;

    SimMotorPolicy(SimMotorTask::Queue &queue)
        : SimTMC2130Policy(), _callback(), _task_queue(queue) {}

    // Functionality to fulfill concept

    auto lid_stepper_set_dac(uint8_t dac_val) -> void { _dac_val = dac_val; }

    // Simulates the movement occuring immediately
    auto lid_stepper_start(int32_t steps, bool overdrive) -> void {
        auto current_position = current_lid_angle();

        // First, check if a switch is already triggered
        if (!overdrive) {
            if (steps > 0 && open_switch_triggered(current_position)) {
                send_lid_done();
                return;
            }
            if (steps < 0 && close_switch_triggered(current_position)) {
                send_lid_done();
                return;
            }
        }

        auto target_angle = current_position +
                            motor_util::LidStepper::microsteps_to_angle(steps);
        // Now, check if the movement will trigger a switch
        if (!overdrive) {
            if (steps > 0) {
                if (current_position < open_switch_pos_angle &&
                    target_angle > open_switch_pos_angle - switch_width_angle) {
                    target_angle = open_switch_pos_angle;
                }
            } else {
                if (current_position > close_switch_pos_angle &&
                    target_angle <
                        close_switch_pos_angle + switch_width_angle) {
                    target_angle = close_switch_pos_angle;
                }
            }
        }
        // Change the angle and send the response
        auto target_steps =
            motor_util::LidStepper::angle_to_microsteps(target_angle);
        _lid_step_position =
            std::clamp<int32_t>(target_steps, min_lid_steps, max_lid_steps);
        send_lid_done();
    }
    auto lid_stepper_stop() -> void { return; }

    auto lid_stepper_check_fault() -> bool { return false; }

    auto lid_stepper_reset() -> bool {
        _dac_val = 0;
        return true;
    }

    auto lid_solenoid_disengage() -> void { _solenoid_engaged = false; }
    auto lid_solenoid_engage() -> void { _solenoid_engaged = true; }

    // Check based on the current angle position
    auto lid_read_closed_switch() -> bool {
        return close_switch_triggered(current_lid_angle());
    }

    auto lid_read_open_switch() -> bool {
        return open_switch_triggered(current_lid_angle());
    }

    auto seal_stepper_start(Callback cb) -> bool {
        if (_seal_moving) {
            return false;
        }

        _seal_moving = true;
        _callback = cb;
        return true;
    }

    auto seal_stepper_stop() -> void { _seal_moving = false; }

    auto seal_switch_set_armed() -> void { _seal_switch_armed = true; }

    auto seal_switch_set_disarmed() -> void { _seal_switch_armed = false; }

    auto seal_read_limit_switch() -> bool { return false; }

    // For simulation
    auto tick() -> void {
        if (_seal_moving) {
            _callback();
        }
    }

  private:
    // Lowest position the lid can move before stalling
    static constexpr uint32_t min_lid_steps =
        motor_util::LidStepper::angle_to_microsteps(-2.0);
    // Max position for the lid before stalling
    static constexpr uint32_t max_lid_steps =
        motor_util::LidStepper::angle_to_microsteps(120.0);
    // Position of center of closed switch
    static constexpr double close_switch_pos_angle = 0.0F;
    // Position of center of open switch
    static constexpr double open_switch_pos_angle = 90.0F;
    // Simulated width of each lid switch in degrees
    static constexpr double switch_width_angle = 1.0F;

    // Check if open switch is triggered
    [[nodiscard]] static auto open_switch_triggered(double angle) -> bool {
        return std::abs(angle - open_switch_pos_angle) <= switch_width_angle;
    }

    // Check if close switch is triggered
    [[nodiscard]] static auto close_switch_triggered(double angle) -> bool {
        return std::abs(angle - close_switch_pos_angle) <= switch_width_angle;
    }

    // Convert _lid_step_position from steps to angle
    [[nodiscard]] auto current_lid_angle() const -> double {
        return motor_util::LidStepper::microsteps_to_angle(_lid_step_position);
    }

    auto send_lid_done() -> void {
        static_cast<void>(_task_queue.try_send(messages::LidStepperComplete()));
    }

    // Solenoid is engaged when unpowered
    bool _solenoid_engaged = true;
    uint8_t _dac_val = 0;
    int32_t _lid_step_position = 0;
    bool _seal_moving = false;
    bool _seal_switch_armed = false;
    Callback _callback;
    SimMotorTask::Queue &_task_queue;
};

struct motor_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimMotorTask::Queue()), task(SimMotorTask(queue)) {}
    SimMotorTask::Queue queue;
    SimMotorTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimMotorPolicy(tcb->queue);
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
