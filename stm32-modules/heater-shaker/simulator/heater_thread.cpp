#include "simulator/heater_thread.hpp"

#include <memory>
#include <stop_token>
#include <thread>

#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/tasks.hpp"

using namespace heater_thread;

struct SimHeaterPolicy {
    [[nodiscard]] auto power_good() const -> bool { return true; }
    [[nodiscard]] auto try_reset_power_good() -> bool { return true; };
    auto set_power_output(double relative_power) -> void {
        power = relative_power;
    };
    auto disable_power_output() -> void { power = 0; }

  private:
    double power = 0;
};

struct heater_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimHeaterTask::Queue()), task(SimHeaterTask(queue)) {}
    SimHeaterTask::Queue queue;
    SimHeaterTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    auto policy = SimHeaterPolicy();
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        try {
            tcb->task.run_once(policy);
        } catch (const SimHeaterTask::Queue::StopDuringMsgWait& sdmw) {
            return;
        }
    }
}

auto heater_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimHeaterTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
