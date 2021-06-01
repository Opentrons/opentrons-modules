#include "simulator/comm_thread.hpp"

#include <iostream>
#include <memory>
#include <stop_token>
#include <string_view>
#include <thread>

#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"

using namespace comm_thread;

struct comm_thread::TaskControlBlock {
    TaskControlBlock()
        : queue(SimCommTask::Queue()), task(SimCommTask(queue)) {}
    SimCommTask::Queue queue;
    SimCommTask task;
};

auto run(std::stop_token st, std::shared_ptr<TaskControlBlock> tcb) -> void {
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

auto comm_thread::handle_input(tasks::Tasks<SimulatorMessageQueue>& tasks)
    -> void {
    auto linebuf = std::make_shared<std::string>(1024, 'c');
    while (true) {
        if (!std::cin.getline(linebuf->data(), linebuf->size() - 1, '\n')) {
            return;
        }
        auto wrote_to = std::cin.gcount();
        linebuf->at(wrote_to - 1) = '\n';
        auto message = messages::IncomingMessageFromHost(
            linebuf->data(), linebuf->data() + wrote_to);
        static_cast<void>(tasks.comms->get_message_queue().try_send(message));
    }
}
