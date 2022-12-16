/*
 * the primary interface to the host communications task
 */
#pragma once

#include <concepts>
#include <variant>

#include "core/ack_cache.hpp"
#include "core/version.hpp"
#include "core/xt1511.hpp"
#include "hal/message_queue.hpp"
#include "systemwide.h"
#include "thermocycler-gen2/colors.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace system_task {

// Structure to hold runtime info for pulsing an LED.
// The LED is run with a psuedo-pwm to modulate the brightness, and
// to provide a smooth triangular pulse
class Pulse {
  private:
    // This gives a pleasant visual effect
    static constexpr uint32_t default_period = 25;
    const uint32_t _period;
    uint8_t _pwm = 0;
    uint8_t _count = 0;
    int8_t _direction = 1;

  public:
    explicit Pulse(uint32_t period = default_period) : _period(period) {}

    auto reset() -> void {
        _count = 0;
        _pwm = 0;
    }

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
    [[nodiscard]] auto pwm() const -> uint8_t { return _pwm; }
};

// Class to hold runtime info for blinking the front button. This steps
// through a preprogrammed sequence where the front LED blinks twice and
// then holds steady.
class FrontButtonBlink {
  private:
    // In milliseconds
    static constexpr uint32_t off_time = 200;
    // In milliseconds
    static constexpr uint32_t on_time = 200;
    // Number of repetitions
    static constexpr uint32_t repetitions = 2;
    // Max ticks is precalculated
    static constexpr uint32_t ticks_per_rep = off_time + on_time;
    static constexpr uint32_t total_ticks = ticks_per_rep * repetitions;

    uint32_t _count = 0;

  public:
    auto reset() -> void { _count = 0; }

    [[nodiscard]] auto tick() -> bool {
        _count = std::min(_count + 1, total_ticks);
        if (_count == total_ticks) {
            return true;
        }
        return (_count % ticks_per_rep) > off_time;
    }
};

// Class to hold runtime info for pressing the front button. The button
// has a
class ButtonPress {
  public:
    // The completion callback accepts one parameter, a bool for whether this
    // was a LONG (true) or SHORT (false) button press.
    using Callback = std::function<void(bool)>;

  private:
    // The callback for when the press is completed
    Callback _send_press;
    // The number of milliseconds the button must be held to count as
    // a "long press"
    const uint32_t _long_press_threshold;
    // Number of milliseconds the button has been held for
    uint32_t _ms_count = 0;
    // Whether a message has been sent since the last reset
    bool _press_sent = false;

  public:
    explicit ButtonPress(Callback cb, size_t long_press_threshold)
        : _send_press(std::move(cb)),
          _long_press_threshold(long_press_threshold) {}

    /**
     * @brief Resets the state of the button press. This should be called
     * when the button is initially pressed (e.g. when the IRQ for the button
     * is triggered).
     */
    auto reset() -> void {
        _ms_count = 0;
        _press_sent = false;
    }

    /**
     * @brief Update the button state while it is being held. If the amount
     * of time it has been held exceeds the threshold for being a long press,
     * this function will call the callback and mark that a message was sent.
     *
     * @param delta_ms The number of milliseconds that has passed since the
     * last time this or `reset()` was invoked.
     */
    auto update_held(uint32_t delta_ms) -> void {
        if (!_press_sent) {
            _ms_count += delta_ms;
            if (_ms_count >= _long_press_threshold) {
                // Did  cross the long threshold, so signal a short press.
                _send_press(true);
                _press_sent = true;
            }
        }
    }

    auto released(uint32_t delta_ms) -> void {
        update_held(delta_ms);
        if (!_press_sent) {
            // Did not cross the long threshold, so signal a short press.
            _send_press(false);
            _press_sent = true;
        }
    }
};

using PWM_T = uint16_t;

template <typename Policy>
concept SystemExecutionPolicy = requires(Policy& p, const Policy& cp) {
    {p.enter_bootloader()};
    {
        p.set_serial_number(std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
            "TESTSNXxxxxxxxxxxxxxxxx"})
        } -> std::same_as<errors::ErrorCode>;
    {
        p.get_serial_number()
        } -> std::same_as<std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>>;
    // A function to read the current status of the front button
    { p.get_front_button_status() } -> std::same_as<bool>;
    // A function to set the LED on the front button on or off
    { p.set_front_button_led(true) } -> std::same_as<void>;
};

struct LedState {
    // Configured color of the LED, assuming
    xt1511::XT1511 color = colors::get_color(colors::Colors::WHITE);
    colors::Mode mode = colors::Mode::SOLID;
    // Utility counter for updating state in non-solid modes
    uint32_t counter = 0;
    // Period for movement in MS
    uint32_t period = 0;
};

using Message = messages::SystemMessage;

// By using a template template parameter here, we allow the code instantiating
// this template to do so as SystemTask<SomeQueueImpl> rather than
// SystemTask<SomeQueueImpl<Message>>
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class SystemTask {
    using BootloaderPrepAckCache =
        AckCache<3, messages::ForceUSBDisconnectMessage,
                 messages::DeactivateLidHeatingMessage,
                 messages::DeactivatePlateMessage>;

  public:
    using Queue = QueueImpl<Message>;
    using PlateState = messages::UpdatePlateState::PlateState;
    using MotorState = messages::UpdateMotorState::MotorState;
    // Time between each write to the LED strip
    static constexpr uint32_t LED_UPDATE_PERIOD_MS = 13;
    // Time between each write to the front button
    static constexpr uint32_t FRONT_BUTTON_PERIOD_MS = 1;
    // Total max PWM count for the front button pulsing
    static constexpr uint32_t FRONT_BUTTON_MAX_PULSE = 20;
    // Time that each full "pulse" action should take (sine wave)
    static constexpr uint32_t LED_PULSE_PERIOD_MS = 1000;
    // Max brightness to set for automatic LED actions
    static constexpr uint8_t LED_MAX_BRIGHTNESS = 0x20;
    // Number of milliseconds to consider a button press "long" is 3 seconds
    static constexpr uint32_t LONG_PRESS_TIME_MS = 3000;

    explicit SystemTask(Queue& q)
        : _message_queue(q),
          _task_registry(nullptr),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _prep_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _leds(xt1511::Speed::HALF),
          _led_state{.color = colors::get_color(colors::Colors::WHITE),
                     .mode = colors::Mode::SOLID,
                     .counter = 0,
                     .period = LED_PULSE_PERIOD_MS},
          _led_update_pending(false),
          _plate_error(errors::ErrorCode::NO_ERROR),
          _lid_error(errors::ErrorCode::NO_ERROR),
          _motor_error(errors::ErrorCode::NO_ERROR),
          _plate_state(PlateState::IDLE),
          _motor_state(MotorState::IDLE),
          _front_button_pulse(FRONT_BUTTON_MAX_PULSE),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _front_button_blink(),
          _front_button_last_state(false),
          _light_debug_mode(false) {}
    SystemTask(const SystemTask& other) = delete;
    auto operator=(const SystemTask& other) -> SystemTask& = delete;
    SystemTask(SystemTask&& other) noexcept = delete;
    auto operator=(SystemTask&& other) noexcept -> SystemTask& = delete;
    ~SystemTask() = default;
    auto get_message_queue() -> Queue& { return _message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        _task_registry = other_tasks;
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());
        _message_queue.recv(&message);

        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const messages::EnterBootloaderMessage& message,
                       Policy& policy) -> void {
        // When we go into the bootloader, we're going to do a system reset
        // essentially - we want to undo our clock setup, gate off clocks to
        // peripherals, in general try and make the system look like it just
        // booted. We'd like to not abruptly shut off a bunch of hardware when
        // this happens, so let's try and turn off the rest of the hardware
        // nicely just in case.
        auto disconnect_message = messages::ForceUSBDisconnectMessage{.id = 0};
        disconnect_message.id = _prep_cache.add(disconnect_message);
        if (!_task_registry->comms->get_message_queue().try_send(
                disconnect_message, 1)) {
            _prep_cache.remove_if_present(disconnect_message.id);
        }

        auto plate_message =
            messages::DeactivatePlateMessage{.id = 0, .from_system = true};
        plate_message.id = _prep_cache.add(plate_message);
        if (!_task_registry->thermal_plate->get_message_queue().try_send(
                plate_message, 1)) {
            _prep_cache.remove_if_present(plate_message.id);
        }

        auto lid_message =
            messages::DeactivateLidHeatingMessage{.id = 0, .from_system = true};
        lid_message.id = _prep_cache.add(lid_message);
        if (!_task_registry->lid_heater->get_message_queue().try_send(
                lid_message, 1)) {
            _prep_cache.remove_if_present(lid_message.id);
        }

        auto ack_message =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            ack_message, 1));

        // Somehow we couldn't send any of the messages, maybe system deadlock?
        // Enter bootloader regardless
        if (_prep_cache.empty()) {
            _leds.set_all(xt1511::XT1511());
            static_cast<void>(_leds.write(policy));
            policy.enter_bootloader();
        }
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const messages::AcknowledgePrevious& message,
                       Policy& policy) -> void {
        // handle an acknowledgement for one of the prep tasks we've dispatched
        auto cache_entry =
            _prep_cache.remove_if_present(message.responding_to_id);
        // See if the ack has an error in it so we can forward if necessary
        auto error_result = std::visit(
            [&policy, &message](auto cache_element) -> errors::ErrorCode {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT;
                }
                return message.with_error;
            },
            cache_entry);
        if (error_result != errors::ErrorCode::NO_ERROR) {
            auto error_message = messages::ErrorMessage{.code = error_result};
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(
                    error_message, 1));
        }
        // No remaining setup tasks, enter bootloader
        if (_prep_cache.empty()) {
            _leds.set_all(xt1511::XT1511());
            static_cast<void>(_leds.write(policy));
            policy.enter_bootloader();
        }
    }

    template <typename Policy>
    auto visit_message(const messages::SetSerialNumberMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        response.with_error = policy.set_serial_number(msg.serial_number);
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::GetSystemInfoMessage& msg,
                       Policy& policy) -> void {
        auto serial_number = policy.get_serial_number();

        auto response = messages::GetSystemInfoResponse{
            .responding_to_id = msg.id,
            .serial_number = serial_number,
            .fw_version = version::fw_version(),
            .hw_version = version::hw_version()};
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const messages::UpdateUIMessage& message, Policy& policy)
        -> void {
        static_cast<void>(message);
        static constexpr double TWO = 2.0F;
        _led_state.counter += LED_UPDATE_PERIOD_MS;
        if (_led_state.counter > _led_state.period) {
            _led_state.counter = 0;
        }

        _led_update_pending = false;

        // The LED mode is automatic based on the plate status and error status
        update_led_mode_from_system();

        switch (_led_state.mode) {
            case colors::Mode::SOLID:
                // Don't bother with timer
                _leds.set_all(_led_state.color);
                break;
            case colors::Mode::PULSING: {
                // Set color as a triangle wave
                double brightness = 0.0F;
                if (_led_state.counter < (_led_state.period / 2)) {
                    brightness = static_cast<double>(_led_state.counter) /
                                 (static_cast<double>(_led_state.period) / TWO);
                } else {
                    auto inverse_count =
                        std::abs(static_cast<int>(_led_state.period) -
                                 static_cast<int>(_led_state.counter));
                    brightness = static_cast<double>(inverse_count) /
                                 (static_cast<double>(_led_state.period) / TWO);
                }
                auto color = _led_state.color;
                color.set_scale(brightness);
                _leds.set_all(color);
                break;
            }
            case colors::Mode::BLINKING:
                // Turn on color for half of the duty cycle, off for the other
                // half
                if (_led_state.counter < (_led_state.period / 2)) {
                    _leds.set_all(_led_state.color);
                } else {
                    _leds.set_all(xt1511::XT1511{});
                }
                break;
            case colors::Mode::WIPE: {
                static constexpr size_t trail_length = SYSTEM_LED_COUNT;
                static constexpr size_t head_max = (SYSTEM_LED_COUNT * 2);
                auto percent_done = static_cast<double>(_led_state.counter) /
                                    static_cast<double>(_led_state.period);
                auto head_position = static_cast<size_t>(
                    static_cast<double>(head_max) * percent_done);
                for (size_t i = 0; i < SYSTEM_LED_COUNT; ++i) {
                    if ((i < head_position -
                                 std::min(trail_length, head_position)) ||
                        (i > head_position)) {
                        _leds.pixel(i) = xt1511::XT1511{};
                    } else {
                        _leds.pixel(i) = _led_state.color;
                    }
                }
                break;
            }
        }
        static_cast<void>(_leds.write(policy));
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::SetLedMode& message, Policy& policy)
        -> void {
        static_cast<void>(policy);
        _led_state.color = colors::get_color(message.color);
        _led_state.mode = message.mode;
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::UpdateTaskErrorState& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        using Tasks = messages::UpdateTaskErrorState::Tasks;
        switch (message.task) {
            case Tasks::THERMAL_PLATE:
                _plate_error = message.current_error;
                break;
            case Tasks::THERMAL_LID:
                _lid_error = message.current_error;
                break;
            case Tasks::MOTOR:
                _motor_error = message.current_error;
                break;
        }
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::UpdatePlateState& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        _plate_state = message.state;
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::UpdateMotorState& message,
                       Policy& policy) -> void {
        if (message.state != _motor_state) {
            switch (message.state) {
                case MotorState::IDLE:
                    policy.set_front_button_led(true);
                    break;
                case MotorState::OPENING_OR_CLOSING:
                    _front_button_pulse.reset();
                    break;
                case MotorState::PLATE_LIFT:
                    _front_button_blink.reset();
            }
        }
        _motor_state = message.state;
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::GetFrontButtonMessage& message,
                       Policy& policy) {
        auto response = messages::GetFrontButtonResponse{
            .responding_to_id = message.id,
            .button_pressed = policy.get_front_button_status()};
        static_cast<void>(
            _task_registry->comms->get_message_queue().try_send(response));
    }

    template <SystemExecutionPolicy Policy>
    auto visit_message(const messages::SetLightsDebugMessage& message,
                       Policy& policy) {
        std::ignore = policy;
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        _light_debug_mode = message.enable;
        static_cast<void>(
            _task_registry->comms->get_message_queue().try_send(response));
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const std::monostate& message, Policy& policy) -> void {
        static_cast<void>(message);
        static_cast<void>(policy);
    }

    // Should be provided to LED Timer to send LED Update messages. Ensure that
    // the timer implementation does NOT execute in an interrupt context.
    auto led_timer_callback() -> void {
        if (!_led_update_pending) {
            auto ret = _message_queue.try_send(messages::UpdateUIMessage());
            if (ret) {
                _led_update_pending = true;
            }
        }
    }

    // Should be provided to the front button timer to send Front Button
    // messages. Ensure the timer implementation does NOT execute in an
    // interrupt context.
    auto front_button_callback(bool long_press) -> void {
        static_cast<void>(_task_registry->motor->get_message_queue().try_send(
            messages::FrontButtonPressMessage{.long_press = long_press}));
    }

    template <SystemExecutionPolicy Policy>
    auto front_button_led_callback(Policy& policy) -> void {
        bool led_state = false;
        switch (_motor_state) {
            case MotorState::IDLE:
                led_state = true;
                break;
            case MotorState::OPENING_OR_CLOSING:
                led_state = _front_button_pulse.tick();
                break;
            case MotorState::PLATE_LIFT:
                led_state = _front_button_blink.tick();
                break;
        }
        if (led_state != _front_button_last_state) {
            _front_button_last_state = led_state;
            policy.set_front_button_led(led_state);
        }
    }

    // To be used for tests
    [[nodiscard]] auto get_led_state() -> LedState& { return _led_state; }

  private:
    // Update current state of the UI based on task errors and plate action
    auto update_led_mode_from_system() -> void {
        using namespace colors;

        if (_light_debug_mode) {
            _led_state.color = get_color(Colors::WHITE);
            _led_state.mode = Mode::SOLID;
            return;
        }

        if (_plate_error != errors::ErrorCode::NO_ERROR ||
            _lid_error != errors::ErrorCode::NO_ERROR ||
            _motor_error != errors::ErrorCode::NO_ERROR) {
            _led_state.color = get_color(Colors::ORANGE);
            _led_state.mode = Mode::BLINKING;
        } else {
            switch (_plate_state) {
                case PlateState::IDLE:
                    _led_state.color = get_color(Colors::WHITE);
                    _led_state.mode = Mode::SOLID;
                    break;
                case PlateState::HEATING:
                    _led_state.color = get_color(Colors::RED);
                    _led_state.mode = Mode::PULSING;
                    break;
                case PlateState::AT_HOT_TEMP:
                    _led_state.color = get_color(Colors::RED);
                    _led_state.mode = Mode::SOLID;
                    break;
                case PlateState::COOLING:
                    _led_state.color = get_color(Colors::BLUE);
                    _led_state.mode = Mode::PULSING;
                    break;
                case PlateState::AT_COLD_TEMP:
                    _led_state.color = get_color(Colors::BLUE);
                    _led_state.mode = Mode::SOLID;
                    break;
            }
        }
    }

    Queue& _message_queue;
    tasks::Tasks<QueueImpl>* _task_registry;
    BootloaderPrepAckCache _prep_cache;
    xt1511::XT1511String<PWM_T, SYSTEM_LED_COUNT> _leds;
    LedState _led_state;
    std::atomic<bool> _led_update_pending;
    // Tracks error state of different tasks
    errors::ErrorCode _plate_error;
    errors::ErrorCode _lid_error;
    errors::ErrorCode _motor_error;
    PlateState _plate_state;
    // Must be atomic because it is used directly in a callback
    // that executes in a different task context
    std::atomic<MotorState> _motor_state;
    Pulse _front_button_pulse;
    FrontButtonBlink _front_button_blink;
    std::atomic<bool> _front_button_last_state;
    // If this is true, set the LED's to all-white no matter what.
    bool _light_debug_mode;
};

};  // namespace system_task
