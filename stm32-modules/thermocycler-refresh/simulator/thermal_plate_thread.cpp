#include "simulator/thermal_plate_thread.hpp"

#include <chrono>
#include <stop_token>

#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace thermal_plate_thread;

struct SimThermalPlatePolicy {
  private:
    bool _enabled = false;

  public:
    auto set_enabled(bool enabled) -> void { _enabled = enabled; }
};

struct thermal_plate_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimThermalPlateTask::Queue()),
          task(SimThermalPlateTask(queue)) {}
    SimThermalPlateTask::Queue queue;
    SimThermalPlateTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimThermalPlatePolicy();
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
        } catch (const SimThermalPlateTask::Queue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto thermal_plate_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimThermalPlateTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
