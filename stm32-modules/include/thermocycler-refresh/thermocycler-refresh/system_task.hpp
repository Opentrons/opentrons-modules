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
#include "thermocycler-refresh/colors.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/tasks.hpp"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace system_task {

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
};

struct LedState {
    // Configured color of the LED, assuming
    xt1511::XT1511 color = colors::get_color(colors::Colors::SOFT_WHITE);
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
        AckCache<3, messages::ForceUSBDisconnectMessage>;

  public:
    using Queue = QueueImpl<Message>;
    using PlateState = messages::UpdatePlateState::PlateState;
    // Time between each write to the LED strip
    static constexpr uint32_t LED_UPDATE_PERIOD_MS = 13;
    // Time that each full "pulse" action should take (sine wave)
    static constexpr uint32_t LED_PULSE_PERIOD_MS = 1000;
    // Max brightness to set for automatic LED actions
    static constexpr uint8_t LED_MAX_BRIGHTNESS = 0x20;
    explicit SystemTask(Queue& q)
        : _message_queue(q),
          _task_registry(nullptr),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _prep_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _leds(xt1511::Speed::HALF),
          _led_state{.color = colors::get_color(colors::Colors::SOFT_WHITE),
                     .mode = colors::Mode::SOLID,
                     .counter = 0,
                     .period = LED_PULSE_PERIOD_MS},
          _plate_error(errors::ErrorCode::NO_ERROR),
          _lid_error(errors::ErrorCode::NO_ERROR),
          _motor_error(errors::ErrorCode::NO_ERROR),
          _plate_state(PlateState::IDLE) {}
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
        auto disconnect_id = _prep_cache.add(disconnect_message);
        disconnect_message.id = disconnect_id;
        if (!_task_registry->comms->get_message_queue().try_send(
                disconnect_message, 1)) {
            _prep_cache.remove_if_present(disconnect_id);
        }

        auto ack_message =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            ack_message, 1));

        // Somehow we couldn't send any of the messages, maybe system deadlock?
        // Enter bootloader regardless
        if (_prep_cache.empty()) {
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
        auto response = messages::GetSystemInfoResponse{
            .responding_to_id = msg.id,
            .serial_number = policy.get_serial_number(),
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
    auto visit_message(const messages::UpdateTaskErrorState& message, Policy& policy)
        -> void  {
        static_cast<void>(policy);
        using Tasks = messages::UpdateTaskErrorState::Tasks;
        switch(message.task) {
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
    auto visit_message(const messages::UpdatePlateState& message, Policy& policy)
        -> void  {
        static_cast<void>(policy);
        _plate_state = message.state;
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
        static_cast<void>(
            get_message_queue().try_send(messages::UpdateUIMessage()));
    }

    // To be used for tests
    [[nodiscard]] auto get_led_state() -> LedState& { return _led_state; }

  private:

    // Update current state of the UI based on task errors and plate action
    auto update_led_mode_from_system() -> void {
        using namespace colors;
        if(_plate_error != errors::ErrorCode::NO_ERROR ||
           _lid_error != errors::ErrorCode::NO_ERROR ||
           _motor_error != errors::ErrorCode::NO_ERROR ) {

            _led_state.color = get_color(Colors::ORANGE);
            _led_state.mode = Mode::BLINKING;
        } else {
            switch(_plate_state) {
                case PlateState::IDLE:
                    _led_state.color = get_color(Colors::SOFT_WHITE);
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
    // Tracks error state of different tasks
    errors::ErrorCode _plate_error;
    errors::ErrorCode _lid_error;
    errors::ErrorCode _motor_error;
    PlateState _plate_state;
};

};  // namespace system_task
