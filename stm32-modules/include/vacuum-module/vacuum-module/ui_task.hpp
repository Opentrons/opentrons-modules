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

// There are 4 channels per color and 4 LEDs for a total of 16 channels
using ChannelMapping = std::array<size_t, 16>;
static constexpr ChannelMapping white_channels{2, 3, 4, 5};
static constexpr ChannelMapping red_channels{6, 9, 12, 15};
static constexpr ChannelMapping green_channels{7, 10, 13, 16};
static constexpr ChannelMapping blue_channels{8, 11, 14, 17};
static constexpr ChannelMapping yellow_channels{6, 7, 9, 10, 12, 13, 15, 16};
static constexpr int LED_CHANNEL_START = 2;
static constexpr int LED_CHANNEL_END = 17;

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
static constexpr uint32_t UPDATE_PERIOD_MS = 10;
static constexpr uint8_t LED_DRIVER0_I2C_ADDRESS = 0x6C << 1;  // Internal
static constexpr auto DEFAULT_COLOR = StatusBarColor::White;
static constexpr auto DEFAULT_POWER = 0.5F;
static constexpr auto SYSTEM_LED_COUNT = 16;

// Time between each write to the LED strip
static constexpr uint32_t LED_UPDATE_PERIOD_MS = 1U * UPDATE_PERIOD_MS;
// Time to fade from one color to the next
static constexpr uint32_t LED_FADE_PERIOD_MS = 500U;
// Time to flash the color on and off
static constexpr uint32_t LED_FLASH_PERIOD_MS = 500U;
// Time that each full "pulse" action should take (sine wave)
static constexpr uint32_t LED_PULSE_PERIOD_MS = 1000U;
// Time that it takes to fade from confirm color to original color
static constexpr uint32_t LED_CONFIRM_FADE_PERIOD_MS = 5000U;
// Time that it takes for the confirm color to flash
static constexpr uint32_t LED_CONFIRM_PERIOD_MS = 300U;
// Time that it takes for the confirm pattern to turn on the led
static constexpr uint32_t LED_CONFIRM_ON_PERIOD_MS =
    LED_CONFIRM_PERIOD_MS + 100;
// Time to blink heartbeat LED
static constexpr uint32_t HB_UPDATE_PERIOD_MS = 500U / UPDATE_PERIOD_MS;
// Max duration of the animation in ms (10 seconds)
static constexpr uint32_t MAX_UPDATE_PERIOD_MS = 10000U;
// Min duration of the animation in ms (25 ms)
static constexpr uint32_t MIN_UPDATE_PERIOD_MS = 25U;
// Reps to signify runnning forever
static constexpr int8_t FOREVER = -1;

struct StatusBarState {
    StatusBarID kind;
    StatusBarColor color;
    StatusBarColor old_color;
    StatusBarPattern pattern = StatusBarPattern::Static;
    float power;
    float power_dt;
    uint32_t duration = LED_PULSE_PERIOD_MS;
    int8_t reps = FOREVER;
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

            // MPR Series
            auto dev_address =
                0x18
                << 1;  // 22 SA0 LPS22DF I2C address select; HI: 0x5D; LO: 0x5C
            // auto PRESSURE  = 0xAA;  // 1 byte
            uint8_t buff[10] = {0};
            uint8_t wr_buff[10] = {0xAA, 0x0, 0x0};
            _policy->i2c_master_write(dev_address, wr_buff, 3);
            _policy->sleep_ms(10);
            _policy->i2c_master_read(dev_address, buff, 4);

            // if (!_led_driver.initialized()) {
            //     _led_driver.initialize(policy);
            //     _led_bar_internal.driver_ok = true;
            //     set_status_bar(Internal);
            // }
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

        // tic the statusbar leds
        update_ui();
    }

    template <UIPolicyIface Policy>
    auto visit_message(const std::monostate& m, Policy& policy) -> void {
        static_cast<void>(m);
        static_cast<void>(policy);
    }

    template <UIPolicyIface Policy>
    auto visit_message(const messages::SetStatusBarStateMessage& m,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        return;
        for (auto bar_id : {StatusBarID::Internal}) {
            bar_id = m.bar_id.value_or(bar_id);
            auto& status_bar = get_statusbar_state(bar_id);
            auto color = m.color.value_or(status_bar.color);
            auto power = m.power.value_or(status_bar.power);
            update_statusbar_state(bar_id, color, power, m.pattern, m.duration,
                                   m.reps);
            // Only set one status bar if one was given.
            if (m.bar_id.has_value()) {
                break;
            }
        }
        if (m.from_host) {
            // Only send ack if this was requested by the host
            static_cast<void>(_task_registry->send_to_address(
                messages::AcknowledgePrevious{.responding_to_id = m.id},
                Queues::HostCommsAddress));
        }
    }

    /*
    Tics the statusbar leds to the next state.
    */
    // NOLINTNEXTLINE[readability-function-cognitive-complexity]
    auto update_ui() -> void {
        static constexpr double TWO = 2.0F;
        for (auto bar_id : {StatusBarID::Internal}) {
            _led_update_pending = false;
            auto& status_bar = get_statusbar_state(bar_id);
            // Skip if driver not initialized or reps reached 0
            if (!status_bar.driver_ok) {
                continue;
            }
            if (status_bar.reps != FOREVER && status_bar.reps < 1) {
                continue;
            }

            // Turn off LEDs when power = 0
            if (status_bar.power == 0) {
                status_bar.reps = 0;
                set_status_bar(bar_id);
                continue;
            }

            status_bar.counter += LED_UPDATE_PERIOD_MS;
            if (status_bar.counter > status_bar.duration) {
                status_bar.counter = 0;
            }

            switch (status_bar.pattern) {
                case StatusBarPattern::Static: {
                    // Fade from current color to new color
                    float power = status_bar.power;
                    if (status_bar.counter < status_bar.duration) {
                        power = static_cast<float>(status_bar.counter) /
                                (static_cast<float>(status_bar.duration));
                        power = std::clamp(power, 0.0F, status_bar.power);
                        set_status_bar(bar_id, status_bar.color, power, true);
                        status_bar.power_dt = power;
                    } else {
                        // Static only happens once
                        status_bar.reps = 0;
                        status_bar.power_dt = 0;
                        return;
                    }
                    break;
                }
                case StatusBarPattern::Pulse: {
                    // Set color as a triangle wave
                    float power = status_bar.power;
                    if (status_bar.counter < (status_bar.duration / TWO)) {
                        power = static_cast<float>(status_bar.counter) /
                                (static_cast<float>(status_bar.duration) / TWO);
                    } else {
                        auto inverse_count =
                            std::abs(static_cast<int>(status_bar.duration) -
                                     static_cast<int>(status_bar.counter));
                        power = static_cast<float>(inverse_count) /
                                (static_cast<float>(status_bar.duration) / TWO);
                    }
                    power = std::clamp(power, 0.0F, status_bar.power);
                    set_status_bar(bar_id, status_bar.color, power);
                    status_bar.power_dt = power;
                    break;
                }
                case StatusBarPattern::Flash: {
                    // Blink the statusbar by turning on/off by half the
                    // duration.
                    auto power =
                        (status_bar.counter < (status_bar.duration / TWO))
                            ? 0
                            : status_bar.power;
                    set_status_bar(bar_id, status_bar.color, power);
                    status_bar.power_dt = power;
                    break;
                }
                case StatusBarPattern::Confirm: {
                    // turn on then off led to new color
                    if (status_bar.counter < LED_CONFIRM_PERIOD_MS) {
                        auto power =
                            (status_bar.counter < LED_CONFIRM_PERIOD_MS / TWO)
                                ? status_bar.power
                                : 0;
                        set_status_bar(bar_id, status_bar.color, power);
                        status_bar.power_dt = power;
                        // turn on led to new color
                    } else if (status_bar.counter < LED_CONFIRM_ON_PERIOD_MS) {
                        set_status_bar(bar_id, status_bar.color,
                                       status_bar.power);
                        // fade to old color
                    } else {
                        float power = status_bar.power;
                        if (status_bar.counter < status_bar.duration) {
                            power = static_cast<float>(status_bar.counter) /
                                    (static_cast<float>(status_bar.duration));
                            power = std::clamp(power, 0.0F, status_bar.power);
                            set_status_bar(bar_id, status_bar.old_color, power,
                                           true);
                            status_bar.power_dt = power;
                        } else {
                            status_bar.reps = 0;
                            status_bar.power_dt = 0;
                            status_bar.color = status_bar.old_color;
                            return;
                        }
                    }
                    break;
                }
            }

            if (status_bar.counter >= status_bar.duration) {
                if (status_bar.reps != FOREVER) {
                    status_bar.reps -= 1;
                }
            }
        }
    }

    /**
     * @brief Set the power (separate from PWM) for a color. Each color has
     * 4 channels, so this helper will set all of the channels.
     */
    auto set_color_power(StatusBarID bar, StatusBarColor color, float power,
                         bool wipe = true) -> bool {
        const auto& channels = color_to_channels(color);

        // clear the LEDs not being set
        if (wipe) {
            for (int i = LED_CHANNEL_START; i <= LED_CHANNEL_END; i++) {
                if (std::find(std::begin(channels), std::end(channels), i) !=
                    std::end(channels)) {
                    continue;
                }
                if (bar == Internal) {
                    _led_driver.set_current(i, 0);
                }
            }
        }

        return std::ranges::all_of(
            channels.cbegin(), channels.cend(), [bar, power, this](size_t c) {
                if (bar == Internal) {
                    return _led_driver.set_current(c, power);
                }
                return false;
            });
    }

    // Helper to get the StatusBarState given the bar id
    auto get_statusbar_state(StatusBarID bar) -> StatusBarState& {
        static_cast<void>(bar);
        return _led_bar_internal;
    }

    auto get_default_period(StatusBarPattern pattern) -> uint32_t {
        switch (pattern) {
            case Flash:
                return LED_FLASH_PERIOD_MS;
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

    auto get_default_reps(StatusBarPattern pattern) -> int8_t {
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
        auto& status_bar = get_statusbar_state(bar);
        // If the old power was 0, set the old color to the new color
        status_bar.old_color = status_bar.power == 0 ? color : status_bar.color;
        status_bar.color = color;
        status_bar.power = std::clamp(power, 0.0F, 1.0F);
        status_bar.pattern = pattern.value_or(status_bar.pattern);
        auto duration_ =
            duration.value_or(get_default_period(status_bar.pattern));
        status_bar.duration = std::clamp(
            (uint32_t)duration_, MIN_UPDATE_PERIOD_MS, MAX_UPDATE_PERIOD_MS);
        status_bar.reps = reps.value_or(get_default_reps(status_bar.pattern));
        status_bar.counter = 0;
    }

    auto set_status_bar(StatusBarID bar,
                        std::optional<StatusBarColor> color = std::nullopt,
                        std::optional<float> power = std::nullopt,
                        std::optional<bool> wipe = std::nullopt) -> bool {
        // Skip if driver not initialized
        auto status_bar = get_statusbar_state(bar);
        if (!status_bar.driver_ok) {
            return false;
        }

        // Skip if there is no state change
        if (power.has_value() and color.has_value()) {
            if (power.value() == status_bar.power_dt &&
                color.value() == status_bar.color) {
                return true;
            }
        }

        auto power_ = power.value_or(status_bar.power);
        auto color_ = color.value_or(status_bar.color);
        auto wipe_ = wipe.value_or(true);
        set_color_power(bar, color_, power_, wipe_);
        if (bar == Internal) {
            _led_driver.set_pwm(power_);
            return _led_driver.send_update(*_policy);
        }
        return false;
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    UIPolicy* _policy;
    is31fl::IS31FL<LED_DRIVER0_I2C_ADDRESS> _led_driver{};
    FreeRTOSTimer _ui_timer;

    StatusBarState _led_bar_internal = led_bar_internal;
    bool _initialized = false;
    bool hb_led_state = false;
    uint32_t hb_counter = 0;
    bool _led_update_pending = false;
};
}  // namespace ui_task
