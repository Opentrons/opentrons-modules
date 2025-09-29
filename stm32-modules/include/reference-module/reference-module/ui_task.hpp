#pragma once

#include <cstdint>

#include "core/is31fl_driver.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"
#include "reference-module/messages.hpp"
#include "ui_policy.hpp"

namespace ui_task {
using namespace ot_utils::freertos_timer;
using namespace ui_policy;

template <typename Policy>
concept UIPolicyIface = requires(Policy& p) {
    // A function to set the heartbeat LED on or off
    {p.set_heartbeat_led(true)};
};

// The timer driving LED update frequency should run at this period
static constexpr uint32_t UPDATE_PERIOD_MS = 10;
// Time to blink heartbeat LED
static constexpr uint32_t HB_UPDATE_PERIOD_MS = 500U / UPDATE_PERIOD_MS;

using Message = messages::UIMessage;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class UITask {
  public:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

    explicit UITask(Queue& q, Aggregator* aggregator, UIPolicy* policy)
        : _message_queue(q),
          _task_registry(aggregator),
          _policy(policy),
          _ui_timer(
              "UI Timer", [ThisPtr = this] { ThisPtr->led_timer_callback(); },
              UPDATE_PERIOD_MS) {
        _ui_timer.start();
    }
    UITask(const UITask& other) = delete;
    auto operator=(const UITask& other) -> UITask& = delete;
    UITask(UITask&& other) noexcept = delete;
    auto operator=(UITask&& other) noexcept -> UITask& = delete;
    ~UITask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <UIPolicyIface Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());

        if (!_initialized) {
            _policy = &policy;
            _message_queue.set_ready();
            _initialized = true;
        }

        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    // Should be provided to LED Timer to send LED Update messages. Ensure that
    // the timer implementation does NOT execute in an interrupt context.
    auto led_timer_callback() -> void {
        // Update heartbeat led
        hb_counter += 1;
        if (hb_counter > HB_UPDATE_PERIOD_MS) {
            hb_led_state = !hb_led_state;
            _policy->set_heartbeat_led(hb_led_state);
            hb_counter = 0;
        }
    }

    template <UIPolicyIface Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    UIPolicy* _policy;
    FreeRTOSTimer _ui_timer;

    bool _initialized = false;
    bool hb_led_state = false;
    uint32_t hb_counter = 0;
};
}  // namespace ui_task
