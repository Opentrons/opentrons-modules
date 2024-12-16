#pragma once

#include <algorithm>
#include <array>

#include "core/is31fl_driver.hpp"
#include "errors.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"
#include "systemwide.h"
#include "ui_policy.hpp"

namespace ui_task {
using namespace ot_utils::freertos_timer;
using namespace ui_policy;

template <typename Policy>
concept UIPolicyIface = requires(Policy& p) {
    // A function to set the heartbeat LED on or off
    {p.set_heartbeat_led(true)};
}
&&is31fl::IS31FL_Policy<Policy>;

// There are 3 channels per color
using ChannelMapping = std::array<size_t, 3>;

static constexpr ChannelMapping white_channels{3, 4, 5};
static constexpr ChannelMapping red_channels{6, 9, 12};
static constexpr ChannelMapping green_channels{7, 10, 13};
static constexpr ChannelMapping blue_channels{8, 11, 14};

static auto color_to_channels(StatusBarColor color) -> const ChannelMapping& {
    switch (color) {
        case StatusBarColor::White:
            return white_channels;
        case StatusBarColor::Red:
            return red_channels;
        case StatusBarColor::Green:
            return green_channels;
        case StatusBarColor::Blue:
            return blue_channels;
        default:
            return white_channels;
    }
}

// The timer driving LED update frequency should run at this period
static constexpr uint32_t UPDATE_PERIOD_MS = 1000;
static constexpr uint8_t LED_DRIVER0_I2C_ADDRESS = 0x6C << 1;  // Internal
static constexpr uint8_t LED_DRIVER1_I2C_ADDRESS = 0x6F << 1;  // External
static constexpr auto DEFAULT_COLOR = StatusBarColor::Green;
static constexpr auto DEFAULT_POWER = 0.5F;

struct StatusBarState {
    StatusBarID kind;
    StatusBarColor color;
    float power;
};

const StatusBarState led_bar_internal = {
    .kind = StatusBarID::Internal,
    .color = DEFAULT_COLOR,
    .power = DEFAULT_POWER,
};

const StatusBarState led_bar_external = {
    .kind = StatusBarID::External,
    .color = DEFAULT_COLOR,
    .power = DEFAULT_POWER,
};

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
              "UI Timer", [ThisPtr = this] { ThisPtr->heartbeat_led(); },
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

            if (!_led_driver0.initialized()) {
                _led_driver0.initialize(policy);
                StatusBarState bar_state = get_statusbar_state(Internal);
                set_status_bar(Internal, bar_state.color, bar_state.power);
            }
            if (!_led_driver1.initialized()) {
                _led_driver1.initialize(policy);
                StatusBarState bar_state = get_statusbar_state(External);
                set_status_bar(External, bar_state.color, bar_state.power);
            }
            _initialized = true;
        }

        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <UIPolicyIface Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <UIPolicyIface Policy>
    auto visit_message(const messages::SetStatusBarColorMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};

        for (auto bar_id : {StatusBarID::Internal, StatusBarID::External}) {
            if (m.bar_id.has_value()) bar_id = m.bar_id.value();
            StatusBarState bar = get_statusbar_state(bar_id);
            StatusBarColor color =
                (m.color.has_value()) ? m.color.value() : bar.color;
            float power = (m.power.has_value()) ? m.power.value() : bar.power;
            if (!set_status_bar(bar_id, color, power)) {
                response.with_error =
                    errors::ErrorCode::SYSTEM_SET_STATUSBAR_COLOR_ERROR;
            }
            // Only set one status bar if one was given.
            if (m.bar_id.has_value()) break;
        }

        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    /**
     * @brief Set the power (separate from PWM) for a color. Each color has
     * 3 channels, so this helper will set all of the channels.
     */
    auto set_color_power(StatusBarID bar, StatusBarColor color, float power)
        -> bool {
        const auto& channels = color_to_channels(color);

        // clear the current leds
        if (bar == Internal) {
            _led_driver0.set_current(0);
        }
        if (bar == External) {
            _led_driver1.set_current(0);
        }

        return std::ranges::all_of(
            channels.cbegin(), channels.cend(), [bar, power, this](size_t c) {
                if (bar == Internal) {
                    return _led_driver0.set_current(c, power);
                }
                if (bar == External) {
                    return _led_driver1.set_current(c, power);
                }
                return false;
            });
    }

    // Callback function for the heartbeat led timer
    auto heartbeat_led() {
        hb_led_state = !hb_led_state;
        _policy->set_heartbeat_led(hb_led_state);
    }

    // Helper to get the StatusBarState given the bar id
    auto get_statusbar_state(StatusBarID bar) -> StatusBarState& {
        switch (bar) {
            case Internal:
                return _led_bar_internal;
            case External:
                return _led_bar_external;
            default:
                return _led_bar_internal;
        }
    }

    auto set_status_bar(StatusBarID bar, StatusBarColor color, float power)
        -> bool {
        power = std::clamp(power, 0.0F, 1.0F);
        auto status_bar = &get_statusbar_state(bar);
        status_bar->color = color;
        status_bar->power = power;

        set_color_power(bar, color, power);
        if (bar == Internal) {
            _led_driver0.set_pwm(power);
            return _led_driver0.send_update(*_policy);
        }
        if (bar == External) {
            _led_driver1.set_pwm(power);
            return _led_driver1.send_update(*_policy);
        }
        return false;
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    UIPolicy* _policy;
    is31fl::IS31FL<LED_DRIVER0_I2C_ADDRESS> _led_driver0{};
    is31fl::IS31FL<LED_DRIVER1_I2C_ADDRESS> _led_driver1{};
    FreeRTOSTimer _ui_timer;

    StatusBarState _led_bar_internal = led_bar_internal;
    StatusBarState _led_bar_external = led_bar_external;
    bool _initialized = false;
    bool hb_led_state = false;
};
};  // namespace ui_task
