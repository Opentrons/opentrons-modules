#pragma once

#include <deque>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
template <typename Message, size_t queue_size = 10>
class TestMessageQueue {
  public:
    std::deque<Message> backing_deque;
    bool act_full;
    std::string name;

    explicit TestMessageQueue(const std::string& name)
        : backing_deque(), act_full(false), name(name) {}

    [[nodiscard]] auto try_send(const Message& message,
                                const uint32_t timeout_ticks = 0) -> bool {
        if (act_full) {
            return false;
        }
        backing_deque.push_back(message);
        return true;
    }

    [[nodiscard]] auto try_recv(Message* message) -> bool {
        if (backing_deque.empty()) {
            return false;
        } else {
            *message = backing_deque.front();
            backing_deque.pop_front();
            return true;
        }
    }

    [[nodiscard]] auto has_message() const -> bool {
        return !backing_deque.empty();
    }
};
