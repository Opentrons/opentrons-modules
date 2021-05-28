#include "simulator/system_thread.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <thread>

#include "heater-shaker/errors.hpp"
#include "heater-shaker/tasks.hpp"

using namespace system_thread;

class SimSystemPolicy {
    auto enter_bootloader() -> void {
        std::terminate();
    }
};

struct system_thread::TaskControlBlock {
    TaskControlBlock() : queue(SimSystemTask::Queue()), task(SimSystemTask(queue)) {}
    SimSystemTask::Queue queue;
    SimSystemTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    auto policy = SimSystemPolicy();
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        tcb->task.run_once(policy);
    }
}

auto system_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimSystemTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
