#pragma once

#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <exception>
#include <limits>
#include <thread>

template <typename Message, size_t queue_size = 8>
class SimulatorMessageQueue {
  public:
    using clock = std::chrono::steady_clock;
    using QueueType =
        boost::lockfree::queue<Message, boost::lockfree::capacity<queue_size>>;
    class StopDuringMsgWait : public std::exception {};
    SimulatorMessageQueue() : queue(queue_size), mythread_stop_token() {}

    auto get_backing_queue() -> QueueType& { return queue; }
    auto set_stop_token(std::stop_token st) { mythread_stop_token = st; }

    [[nodiscard]] auto try_send(const Message& message,
                                const uint32_t timeout_ticks = 0) -> bool {
        auto at_start = clock::now();
        bool sent_message = false;
        while (!sent_message) {
            using namespace std::literals::chrono_literals;
            sent_message = queue.push(message);
            if (sent_message) {
                return sent_message;
            }
            if ((clock::now() - at_start) >
                std::chrono::milliseconds(timeout_ticks)) {
                return false;
            }
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }

    [[nodiscard]] auto try_recv(Message* message, uint32_t timeout_ticks = 0)
        -> bool {
        if (!message) {
            throw std::invalid_argument("null message pointer");
        }
        auto at_start = clock::now();
        bool got_message = false;
        while (!got_message) {
            using namespace std::literals::chrono_literals;
            got_message = queue.pop(*message);
            if (got_message) {
                return got_message;
            }
            if ((clock::now() - at_start) >
                std::chrono::milliseconds(timeout_ticks)) {
                return false;
            }
            if (mythread_stop_token.stop_requested()) {
                throw StopDuringMsgWait();
            }
            std::this_thread::sleep_for(1ms);
        }
        return false;
    }

    auto recv(Message* message) -> void {
        static_cast<void>(
            try_recv(message, std::numeric_limits<uint32_t>::max()));
    }

    [[nodiscard]] auto has_message() const -> bool { return !queue.empty(); }

  private:
    QueueType queue;
    std::stop_token mythread_stop_token;
};
