#include "simulator/comm_thread.hpp"

#include <iostream>
#include <memory>
#include <stop_token>
#include <string_view>
#include <thread>
#include <boost/asio.hpp>

#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/sim_driver.hpp"


using namespace comm_thread;

struct comm_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimCommTask::Queue()), task(SimCommTask(queue)) {}
    SimCommTask::Queue queue;
    SimCommTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
    std::cout << "Running" << std::endl;
    tcb->queue.set_stop_token(st);
    std::string buffer(1024, 'c');
    while (!st.stop_requested()) {
        try {
            auto wrote_to = tcb->task.run_once(buffer.begin(), buffer.end());
            std::cout << std::string_view(buffer.begin(), wrote_to);
        } catch (const SimCommTask::Queue::StopDuringMsgWait sdmw) {
            return;
        }
    }
}

auto comm_thread::build()
    -> tasks::Task<std::unique_ptr<std::jthread>, comm_thread::SimCommTask> {
    auto tcb = std::make_shared<TaskControlBlock>();
    return tasks::Task{std::make_unique<std::jthread>(run, tcb), &tcb->task};
}

void comm_thread::handle_input(sim_driver::SimDriver* driver, tasks::Tasks<SimulatorMessageQueue>& tasks){
    driver->read(tasks);
}
