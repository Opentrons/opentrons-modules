#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <optional>

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
using ChannelMapping = std::array<size_t, 4>;

static constexpr ChannelMapping white_channels{2, 3, 4, 5};
static constexpr ChannelMapping red_channels{6, 9, 12, 15};
static constexpr ChannelMapping green_channels{7, 10, 13, 16};
static constexpr ChannelMapping blue_channels{8, 11, 14, 17};
static constexpr ChannelMapping yellow_channels{6, 7, 12, 13};

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
        case StatusBarColor::Yellow:
            return yellow_channels;
        default:
            return white_channels;
    }
}

// The timer driving LED update frequency should run at this period
static constexpr uint32_t UPDATE_PERIOD_MS = 1;
static constexpr uint8_t LED_DRIVER0_I2C_ADDRESS = 0x6C << 1;  // Internal
static constexpr uint8_t LED_DRIVER1_I2C_ADDRESS = 0x6F << 1;  // External
static constexpr auto DEFAULT_COLOR = StatusBarColor::Green;
static constexpr auto DEFAULT_POWER = 0.5F;
static constexpr auto SYSTEM_LED_COUNT = 16;

// Time between each write to the LED strip
static constexpr uint32_t LED_UPDATE_PERIOD_MS = 5U;
// Time to fade from one color to the next
static constexpr uint32_t LED_FADE_PERIOD_MS = 500U;
// Time that each full "pulse" action should take (sine wave)
static constexpr uint32_t LED_PULSE_PERIOD_MS = 1000U;
// Time that it takes to fade from confirm color to original color
static constexpr uint32_t LED_CONFIRM_FADE_PERIOD_MS = 5000U;
// Time that it takes for the confirm color to flash
static constexpr uint32_t LED_CONFIRM_PERIOD_MS = 300U;
// Time to blink heartbeat LED
static constexpr uint32_t HB_UPDATE_PERIOD_MS = 500U;
// Max duration of the animation in ms (10 seconds)
static constexpr uint32_t MAX_UPDATE_PERIOD_MS = 10000U;
// Min duration of the animation in ms (25 ms)
static constexpr uint32_t MIN_UPDATE_PERIOD_MS = 25U;
// Reps to signify runnning forever
static constexpr int FOREVER = -1;

struct StatusBarState {
    StatusBarID kind;
    StatusBarColor color;
    StatusBarColor old_color;
    StatusBarPattern pattern = StatusBarPattern::Static;
    float power;
    float power_dt;
    uint32_t duration = LED_FADE_PERIOD_MS;
    int reps = 0;
    uint32_t counter = 0;
    bool driver_ok = false;
};

const StatusBarState led_bar_internal = {
    .kind = StatusBarID::Internal,
    .color = DEFAULT_COLOR,
    .old_color = DEFAULT_COLOR,
    .power = DEFAULT_POWER,
    .power_dt = 0,
};

const StatusBarState led_bar_external = {
    .kind = StatusBarID::External,
    .color = DEFAULT_COLOR,
    .old_color = DEFAULT_COLOR,
    .power = DEFAULT_POWER,
    .power_dt = 0,
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

            if (!_led_driver0.initialized()) {
                _led_driver0.initialize(policy);
                _led_bar_internal.driver_ok = true;
                set_status_bar(Internal);
            }
            if (!_led_driver1.initialized()) {
                _led_driver1.initialize(policy);
                _led_bar_external.driver_ok = true;
                set_status_bar(External);
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

        if (!_led_update_pending) {
            auto ret = _message_queue.try_send(messages::UpdateUIMessage());
            if (ret) {
                _led_update_pending = true;
            }
        }
    }

    template <UIPolicyIface Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <UIPolicyIface Policy>
    auto visit_message(const messages::UpdateUIMessage& m, Policy& policy)
        -> void {
        static constexpr double TWO = 2.0F;
        for (auto bar_id : {StatusBarID::Internal, StatusBarID::External}) {
            _led_update_pending = false;
            auto led_bar = &get_statusbar_state(bar_id);
            // Skip if driver not initialized or reps reached 0
            if (!led_bar->driver_ok) continue;
            if (led_bar->reps != FOREVER && led_bar->reps < 1) continue;

            // Turn off LEDs when power = 0
            if (led_bar->power == 0) {
                led_bar->reps = 0;
                set_status_bar(bar_id);
                continue;
            }

            led_bar->counter += LED_UPDATE_PERIOD_MS;
            if (led_bar->counter > led_bar->duration) {
                led_bar->counter = 0;
            }

            switch (led_bar->pattern) {
                case StatusBarPattern::Static: {
                    // Fade from current color to new color
                    float power = led_bar->power;
                    if (led_bar->counter < led_bar->duration) {
                        power = static_cast<float>(led_bar->counter) /
                                (static_cast<float>(led_bar->duration));
                        power = std::clamp(power, 0.0F, led_bar->power);
                        led_bar->power_dt = power;
                        set_status_bar(bar_id, led_bar->color, power, true);
                    } else {
                        // Static only happens once
                        led_bar->reps = 0;
                        led_bar->power_dt = 0;
                        return;
                    }
                    break;
                }
                case StatusBarPattern::Pulse: {
                    // Set color as a triangle wave
                    float power = led_bar->power;
                    if (led_bar->counter < (led_bar->duration / 2)) {
                        power = static_cast<float>(led_bar->counter) /
                                (static_cast<float>(led_bar->duration) / TWO);
                    } else {
                        auto inverse_count =
                            std::abs(static_cast<int>(led_bar->duration) -
                                     static_cast<int>(led_bar->counter));
                        power = static_cast<float>(inverse_count) /
                                (static_cast<float>(led_bar->duration) / TWO);
                    }

                    led_bar->power_dt = std::clamp(power, 0.0F, led_bar->power);
                    set_status_bar(bar_id, led_bar->color, power);
                    break;
                }
                case StatusBarPattern::Flash: {
                    // Blink the statusbar by turning on/off by half the
                    // duration.
                    auto power = (led_bar->counter < (led_bar->duration / 2))
                                     ? 0
                                     : led_bar->power;
                    led_bar->power_dt = power;
                    set_status_bar(bar_id, led_bar->color, power);
                    break;
                }
                case StatusBarPattern::Confirm: {
                    // turn on then off led to new color
                    if (led_bar->counter < LED_CONFIRM_PERIOD_MS) {
                        auto power =
                            (led_bar->counter < LED_CONFIRM_PERIOD_MS / 2)
                                ? led_bar->power
                                : 0;
                        set_status_bar(bar_id, led_bar->color, power);
                        // turn on led to new color
                    } else if (led_bar->counter < LED_CONFIRM_PERIOD_MS + 100) {
                        set_status_bar(bar_id, led_bar->color, led_bar->power);
                        // fade to old color
                    } else {
                        float power = led_bar->power;
                        if (led_bar->counter < led_bar->duration) {
                            power = static_cast<float>(led_bar->counter) /
                                    (static_cast<float>(led_bar->duration));
                            power = std::clamp(power, 0.0F, led_bar->power);
                            led_bar->power_dt = power;
                            set_status_bar(bar_id, led_bar->old_color, power,
                                           true);
                        } else {
                            led_bar->reps = 0;
                            led_bar->power_dt = 0;
                            led_bar->color = led_bar->old_color;
                            return;
                        }
                    }
                    break;
                }
            }

            if (led_bar->counter >= led_bar->duration) {
                if (led_bar->reps != FOREVER) {
                    led_bar->reps -= 1;
                }
            }
        }
    }

    template <UIPolicyIface Policy>
    auto visit_message(const messages::SetStatusBarStateMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        for (auto bar_id : {StatusBarID::Internal, StatusBarID::External}) {
            bar_id = m.bar_id.value_or(bar_id);
            StatusBarState bar = get_statusbar_state(bar_id);
            StatusBarColor color = m.color.value_or(bar.color);
            float power = m.power.value_or(bar.power);
            update_statusbar_state(bar_id, color, power, m.pattern, m.duration,
                                   m.reps);
            // Only set one status bar if one was given.
            if (m.bar_id.has_value()) {
                break;
            }
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    /**
     * @brief Set the power (separate from PWM) for a color. Each color has
     * 3 channels, so this helper will set all of the channels.
     */
    auto set_color_power(StatusBarID bar, StatusBarColor color, float power,
                         bool wipe = true) -> bool {
        const auto& channels = color_to_channels(color);

        // clear the LEDs not being set
        if (wipe) {
            for (int i = 2; i <= 17; i++) {
                if (std::find(std::begin(channels), std::end(channels), i) !=
                    std::end(channels))
                    continue;
                if (bar == Internal) {
                    _led_driver0.set_current(i, 0);
                }
                if (bar == External) {
                    _led_driver1.set_current(i, 0);
                }
            }
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

    auto get_default_period(StatusBarPattern pattern) -> uint32_t {
        switch (pattern) {
            case Flash:
                return LED_FADE_PERIOD_MS;
            case Static:
                return LED_FADE_PERIOD_MS;
            case Pulse:
                return LED_PULSE_PERIOD_MS;
            case Confirm:
                return LED_CONFIRM_FADE_PERIOD_MS;
            default:
                return MAX_UPDATE_PERIOD_MS;
        }
    }

    auto get_default_reps(StatusBarPattern pattern) -> int {
        switch (pattern) {
            case Static:
            case Confirm:
                return 1;
            case Flash:
            case Pulse:
                return FOREVER;
            default:
                return 1;
        }
    }

    auto update_statusbar_state(
        StatusBarID bar, StatusBarColor color, float power,
        std::optional<StatusBarPattern> pattern = std::nullopt,
        std::optional<uint32_t> duration = std::nullopt,
        std::optional<int8_t> reps = std::nullopt) -> void {
        auto status_bar = &get_statusbar_state(bar);
        // If the old power was 0, set the old color to the new color
        status_bar->old_color =
            status_bar->power == 0 ? color : status_bar->color;
        status_bar->color = color;
        status_bar->power = std::clamp(power, 0.0F, 1.0F);
        status_bar->pattern = pattern.value_or(status_bar->pattern);
        auto duration_ =
            duration.value_or(get_default_period(status_bar->pattern));
        status_bar->duration = std::clamp(
            (uint32_t)duration_, MIN_UPDATE_PERIOD_MS, MAX_UPDATE_PERIOD_MS);
        status_bar->reps = reps.value_or(get_default_reps(status_bar->pattern));
        status_bar->counter = 0;
    }

    auto set_status_bar(StatusBarID bar,
                        std::optional<StatusBarColor> color = std::nullopt,
                        std::optional<float> power = std::nullopt,
                        std::optional<bool> wipe = std::nullopt) -> bool {
        // Skip if driver not initialized
        auto status_bar = get_statusbar_state(bar);
        if (!status_bar.driver_ok) return false;

        auto power_ = power.value_or(status_bar.power);
        auto color_ = color.value_or(status_bar.color);
        auto wipe_ = wipe.value_or(true);
        set_color_power(bar, color_, power_, wipe_);
        if (bar == Internal) {
            _led_driver0.set_pwm(power_);
            return _led_driver0.send_update(*_policy);
        }
        if (bar == External) {
            _led_driver1.set_pwm(power_);
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
    uint32_t hb_counter = 0;
    bool _led_update_pending = false;
};
};  // namespace ui_task
