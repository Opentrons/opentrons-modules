/**
 * @file timer.hpp
 * @brief Provides a generic class for one-shot or periodic timer callback
 * functionality.
 */

#pragma once

#include <concepts>
#include <functional>

namespace timer {

/**
 * @brief Defines constraints for a class that implements the low-level
 * aspects of a timer.
 *
 * @tparam Handle The type of the timer handle.
 */
template <typename Handle>
concept TimerHandle = requires(Handle& h, const char* name, uint32_t time_ms,
                               bool autoreload,
                               std::function<void()> callback) {
    // Initializer function
    {Handle(name, time_ms, autoreload, callback)};
    // Start a timer
    { h.start() } -> std::same_as<bool>;
    // Stop a timer
    { h.stop() } -> std::same_as<bool>;
    // Check if a timer is active
    { h.active() } -> std::same_as<bool>;
};

template <TimerHandle Handle>
class GenericTimer {
  public:
    using Callback = std::function<void()>;
    GenericTimer(const char* name, uint32_t period_ms, bool autoreload,
                 Callback callback)
        : _callback(std::move(callback)),
          _handle(name, period_ms, autoreload,
                  std::bind(&GenericTimer<Handle>::callback, this)) {}

    auto start() -> bool { return _handle.start(); }
    auto stop() -> bool { return _handle.stop(); }
    [[nodiscard]] auto const active() -> bool { return _handle.active(); }

    auto callback() -> void { _callback(); }

    [[nodiscard]] auto get_handle() -> Handle& { return _handle; }

  private:
    Callback _callback;
    Handle _handle;
};

}  // namespace timer
