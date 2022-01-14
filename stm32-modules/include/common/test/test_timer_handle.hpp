/**
 * @file test_timer_handle.hpp
 * @brief Implements a mock timer handle for testing the GenericTimer class
 *
 */
#pragma once

#include <functional>

namespace test_timer_handle {

class TestTimerHandle {
  public:
    using Callback = std::function<void()>;
    TestTimerHandle(const char* name, uint32_t time_ms, bool autoreload,
                    Callback callback)
        : _name(name),
          _time_ms(time_ms),
          _autoreload(autoreload),
          _callback(std::move(callback)),
          _active(false),
          _remaining_time(0) {}
    TestTimerHandle(TestTimerHandle&) = delete;
    auto operator=(TestTimerHandle&) -> TestTimerHandle& = delete;

    auto start() -> bool {
        if (_active) {
            return false;
        }
        _active = true;
        _remaining_time = _time_ms;
        return true;
    }

    auto stop() -> bool {
        if (!_active) {
            return false;
        }
        _active = false;
        _remaining_time = 0;
        return true;
    }

    auto tick(uint32_t delta) -> void {
        if (!_active) {
            return;
        }

        if (delta == 0) {
            delta = _remaining_time;
        }

        if (delta < _remaining_time) {
            _remaining_time -= delta;
            return;
        }
        // Timer ends on this tick
        _callback();
        if (_autoreload) {
            _remaining_time = _time_ms;
            delta -= _remaining_time;
            if (delta > 0) {
                // Recurse!
                tick(delta);
            }
        } else {
            // No autoreload means the remaining delta doesn't matter
            _remaining_time = 0;
            _active = false;
        }
    }

    auto active() const -> bool { return _active; }

    auto remaining_time() -> uint32_t { return _remaining_time; }

  private:
    const char* _name;
    uint32_t _time_ms;
    bool _autoreload;
    Callback _callback;

    bool _active;
    uint32_t _remaining_time;
};

/**
 * @brief Mock class to count how many times a function is triggered.
 * Useful for testing Timer classes.
 *
 */
struct InterruptCounter {
    auto increment() -> void { ++_count; }

    auto reset() -> void { _count = 0; }

    [[nodiscard]] auto provide_callback() -> std::function<void()> {
        return std::bind(&InterruptCounter::increment, this);
    }

    uint32_t _count = 0;
};

}  // namespace test_timer_handle
