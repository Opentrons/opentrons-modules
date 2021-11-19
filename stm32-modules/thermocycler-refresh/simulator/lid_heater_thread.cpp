#include "simulator/lid_heater_thread.hpp"

#include <chrono>
#include <stop_token>

#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/tasks.hpp"

using namespace lid_heater_thread;

struct SimLidHeaterPolicy {
  private:
    bool _enabled = false;

  public:
    auto set_enabled(bool enabled) -> void { _enabled = enabled; }
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
