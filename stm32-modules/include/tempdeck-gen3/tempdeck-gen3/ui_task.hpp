#pragma once

#include "hal/message_queue.hpp"
#include "tempdeck-gen3/messages.hpp"
#include "tempdeck-gen3/tasks.hpp"

namespace ui_task {

template <typename Policy>
concept UIPolicy = requires(Policy& p) {
    // A function to set the heartbeat LED on or off
    {p.set_heartbeat_led(true)};
};

// Structure to hold runtime info for the heartbeat LED.
// The LED is run with a psuedo-pwm to confirm that the
// firmware is running and tasks are being called at
// regular intervals
class Heartbeat {
  private:
    // This gives a pleasant visual effect
    static constexpr uint32_t default_period = 25;
    const uint32_t _period;
    uint8_t _pwm = 0;
    uint8_t _count = 0;
    int8_t _direction = 1;

  public:
    explicit Heartbeat(uint32_t period = default_period) : _period(period) {}

    /**
     * @brief Increment heartbeat counter. This provides a pseudo-pwm
     * setup where a "pwm" counter runs from 0 to a configurable period
     * value, and the LED is turned on and off based on whether the repeating
     * counter is below the PWM value.
     *
     * @return true if the LED should be set to on, false if it should
     * be set to off.
     */
    auto tick() -> bool {
        ++_count;
        if (_count == _period) {
            _count = 0;
            _pwm += _direction;
            if (_pwm == _period) {
                _direction = -1;
            } else if (_pwm == 0) {
                _direction = 1;
            }
        }
        return (_pwm > 2) && (_count < _pwm);
    }

    // Get the current pwm period
    auto pwm() -> uint8_t { return _pwm; }
};

using Message = messages::UIMessage;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class UITask {
  public:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

    // The timer driving LED update frequency should run at this period
    static constexpr uint32_t UPDATE_PERIOD_MS = 1;

    explicit UITask(Queue& q, Aggregator* aggregator)
        : _message_queue(q),
          _task_registry(aggregator),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _heartbeat() {}
    UITask(const UITask& other) = delete;
    auto operator=(const UITask& other) -> UITask& = delete;
    UITask(UITask&& other) noexcept = delete;
    auto operator=(UITask&& other) noexcept -> UITask& = delete;
    ~UITask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    // This should be called from the periodic update timer to drive the
    // update frequency of the LED's on the system
    auto update_callback() {
        static_cast<void>(_message_queue.try_send(messages::UpdateUIMessage{}));
    }

    template <UIPolicy Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());
        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <UIPolicy Policy>
    auto visit_message(const std::monostate& message, Policy& policy) -> void {
        static_cast<void>(message);
        static_cast<void>(policy);
    }

    template <UIPolicy Policy>
    auto visit_message(const messages::UpdateUIMessage& message, Policy& policy)
        -> void {
        static_cast<void>(message);
        policy.set_heartbeat_led(_heartbeat.tick());
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    Heartbeat _heartbeat;
};

};  // namespace ui_task
