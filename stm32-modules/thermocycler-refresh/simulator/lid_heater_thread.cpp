#include "simulator/lid_heater_thread.hpp"

#include <chrono>
#include <stop_token>

#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace lid_heater_thread;

class SimLidHeaterPolicy {
  private:
    double _power = 0.0F;

  public:
    auto set_heater_power(double power) -> bool {
        _power = std::clamp(power, (double)0.0F, (double)1.0F);
        return true;
    }

    auto get_heater_power() const -> double { return _power; }
};

struct lid_heater_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimLidHeaterTask::Queue()), task(SimLidHeaterTask(queue)) {}
    SimLidHeaterTask::Queue queue;
    SimLidHeaterTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimLidHeaterPolicy();
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
        } catch (const SimLidHeaterTask::Queue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto lid_heater_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimLidHeaterTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
