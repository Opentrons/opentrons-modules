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
    boost::asio::io_service io_context;

    boost::asio::ip::tcp::socket mysocket(io_context);
    boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 9999);
    boost::system::error_code ec;
    mysocket.connect(endpoint, ec);
    if (ec) {

    }

    char pBuff[30];
    boost::asio::mutable_buffer buff(pBuff, sizeof(pBuff));
    std::string tot;

    std::size_t l = mysocket.read_some(buff);
    while (l > 0) {
        tot.append(static_cast<const char*>(buff.data()), l);

        std::size_t pos = tot.find("\r");
        while (pos != std::string::npos) {
            std::string msg = tot.substr(0, pos);
            tot.erase(0, pos+1);
            std::cout << "Sending " << msg << " to queue" << std::endl;
            auto c_string = msg.c_str();

            auto message = messages::IncomingMessageFromHost(c_string, msg.c_str());
            static_cast<void>(tasks.comms->get_message_queue().try_send(message));
            pos = tot.find("\r");
        }
        l = mysocket.read_some(buff);
    }
}
