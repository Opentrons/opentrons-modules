#include "simulator/ui_thread.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <stop_token>
#include <thread>

#include "heater-shaker/errors.hpp"
#include "heater-shaker/motor_task.hpp"
#include "heater-shaker/tasks.hpp"

using namespace ui_thread;

struct ui_thread::TaskControlBlock {
    TaskControlBlock() : queue(SimUITask::Queue()), task(SimUITask(queue)) {}
    SimUITask::Queue queue;
    SimUITask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    using namespace std::literals::chrono_literals;
    tcb->queue.set_stop_token(st);
    while (!st.stop_requested()) {
        std::this_thread::sleep_for(100ms);
    }
}

auto ui_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, SimUITask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task(std::make_unique<std::jthread>(run, tcb), &tcb->task);
}
