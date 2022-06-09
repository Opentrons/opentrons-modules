#pragma once

#include <deque>
#include <stdexcept>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
template <typename M, size_t Index = 0, size_t queue_size = 10>
class TestMessageQueue {
  public:
    using Message = M;
    std::deque<Message> backing_deque;
    bool act_full;
    std::string name;
    const static size_t index = Index;

    struct Tag {};

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

    [[nodiscard]] auto try_send_from_isr(const Message& message,
                                         const uint32_t timeout_ticks = 0)
        -> bool {
        return try_send(message, timeout_ticks);
    }

    [[nodiscard]] auto try_recv(Message* message, uint32_t timeout_ticks = 0)
        -> bool {
        if (backing_deque.empty()) {
            return false;
        } else {
            *message = backing_deque.front();
            backing_deque.pop_front();
            return true;
        }
    }

    auto recv(Message* message) -> void {
        if (backing_deque.empty()) {
            throw new std::runtime_error(
                "don't do something that calls recv() with an empty buffer");
        }
        *message = backing_deque.front();
        backing_deque.pop_front();
    }

    [[nodiscard]] auto has_message() const -> bool {
        return !backing_deque.empty();
    }
};
