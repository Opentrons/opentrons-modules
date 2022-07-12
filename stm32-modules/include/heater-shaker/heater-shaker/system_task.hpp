/*
 * the primary interface to the host communications task
 */
#pragma once

#include <concepts>
#include <variant>

#include "core/ack_cache.hpp"
#include "core/version.hpp"
#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "systemwide.h"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace system_task {

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
    {
        p.start_set_led(LED_COLOR::WHITE, 255)
        } -> std::same_as<errors::ErrorCode>;
};

struct LEDState {
    double led_tick_count = 0.0;
    LED_COLOR current_color = LED_COLOR::WHITE;
    LED_MODE current_mode = LED_MODE::SOLID_HOLDING;
    bool led_alternate_colors = false;
    LED_COLOR led_color_1 = LED_COLOR::OFF;
    LED_COLOR led_color_2 = LED_COLOR::OFF;
    bool pulse_complete = false;
    LED_COLOR previous_color = LED_COLOR::OFF;
    LED_MODE previous_mode = LED_MODE::MODE_OFF;
};

using Message = messages::SystemMessage;

// By using a template template parameter here, we allow the code instantiating
// this template to do so as SystemTask<SomeQueueImpl> rather than
// SystemTask<SomeQueueImpl<Message>>
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class SystemTask {
    using BootloaderPrepAckCache =
        AckCache<3, messages::SetTemperatureMessage, messages::SetRPMMessage,
                 messages::ForceUSBDisconnectMessage>;

  public:
    using Queue = QueueImpl<Message>;
    static constexpr uint32_t LED_UPDATE_PERIOD_MS =
        25;  // FreeRTOS timer period
    static constexpr double LED_TICKS_PER_PULSE = 80.0;  // 25ms tick
    static constexpr double LED_FULL_SCALE = 255.0;
    static constexpr double ONE = 1.0F;
    static constexpr double TWO = 2.0F;
    explicit SystemTask(Queue& q)
        : message_queue(q),
          task_registry(nullptr),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          prep_cache(),
          _led_state{.led_tick_count = 0.0,
                     .current_color = LED_COLOR::WHITE,
                     .current_mode = LED_MODE::SOLID_HOLDING,
                     .led_alternate_colors = false,
                     .led_color_1 = LED_COLOR::OFF,
                     .led_color_2 = LED_COLOR::OFF,
                     .pulse_complete = false,
                     .previous_color = LED_COLOR::OFF,
                     .previous_mode = LED_MODE::MODE_OFF} {}
    SystemTask(const SystemTask& other) = delete;
    auto operator=(const SystemTask& other) -> SystemTask& = delete;
    SystemTask(SystemTask&& other) noexcept = delete;
    auto operator=(SystemTask&& other) noexcept -> SystemTask& = delete;
    ~SystemTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    [[nodiscard]] auto get_led_mode() const -> LED_MODE {
        return _led_state.current_mode;
    }
    [[nodiscard]] auto get_led_color() const -> LED_COLOR {
        return _led_state.current_color;
    }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());

        message_queue.recv(&message);

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
        auto stop_message = messages::SetRPMMessage{
            .id = 0, .target_rpm = 0, .from_system = true};
        auto stop_id = prep_cache.add(stop_message);
        stop_message.id = stop_id;
        if (!task_registry->motor->get_message_queue().try_send(stop_message,
                                                                1)) {
            prep_cache.remove_if_present(stop_id);
        }

        auto cool_message = messages::SetTemperatureMessage{
            .id = 0, .target_temperature = 0, .from_system = true};
        auto cool_id = prep_cache.add(cool_message);
        cool_message.id = cool_id;
        if (!task_registry->heater->get_message_queue().try_send(cool_message,
                                                                 1)) {
            prep_cache.remove_if_present(cool_id);
        }

        auto disconnect_message = messages::ForceUSBDisconnectMessage{.id = 0};
        auto disconnect_id = prep_cache.add(disconnect_message);
        disconnect_message.id = disconnect_id;
        if (!task_registry->comms->get_message_queue().try_send(
                disconnect_message, 1)) {
            prep_cache.remove_if_present(disconnect_id);
        }

        auto ack_message =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(
            task_registry->comms->get_message_queue().try_send(ack_message, 1));

        // Somehow we couldn't send any of the messages, maybe system deadlock?
        // Enter bootloader regardless
        if (prep_cache.empty()) {
            policy.enter_bootloader();
        }
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const messages::AcknowledgePrevious& message,
                       Policy& policy) -> void {
        // handle an acknowledgement for one of the prep tasks we've dispatched
        auto cache_entry =
            prep_cache.remove_if_present(message.responding_to_id);
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
                task_registry->comms->get_message_queue().try_send(
                    error_message, 1));
        }
        // No remaining setup tasks, enter bootloader
        if (prep_cache.empty()) {
            policy.enter_bootloader();
        }
    }

    template <typename Policy>
    auto visit_message(const messages::SetSerialNumberMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        response.with_error = policy.set_serial_number(msg.serial_number);
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
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
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::SetLEDMessage& msg, Policy& policy)
        -> void {
        _led_state.current_color = msg.color;
        _led_state.current_mode = LED_MODE::SOLID_HOLDING;

        if (msg.from_host) {
            auto response =
                messages::AcknowledgePrevious{.responding_to_id = msg.id};
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    auto visit_message(const messages::IdentifyModuleStartLEDMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
        _led_state.previous_color = _led_state.current_color;
        _led_state.previous_mode = _led_state.current_mode;
        auto message = messages::UpdateLEDStateMessage{};
        if ((_led_state.current_color == AMBER) ||
            (_led_state.current_color == RED_AMBER)) {
            message.color = WHITE_AMBER;
        } else if (_led_state.current_mode == LED_MODE::SOLID_HOT) {
            message.color = RED_WHITE;
        } else {
            message.color = WHITE;
        }
        message.mode = LED_MODE::PULSE;
        static_cast<void>(
            task_registry->system->get_message_queue().try_send(message));
    }

    template <typename Policy>
    auto visit_message(const messages::IdentifyModuleStopLEDMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
        auto message =
            messages::UpdateLEDStateMessage{.color = _led_state.previous_color,
                                            .mode = _led_state.previous_mode};
        static_cast<void>(
            task_registry->system->get_message_queue().try_send(message));
    }

    template <typename Policy>
    auto visit_message(const messages::UpdateLEDStateMessage& msg,
                       Policy& policy) -> void {
        if ((msg.color == LED_COLOR::AMBER) &&
            (_led_state.current_mode == LED_MODE::SOLID_HOT)) {
            _led_state.current_color = LED_COLOR::RED_AMBER;
            _led_state.current_mode = LED_MODE::PULSE;
        } else if ((_led_state.current_color == LED_COLOR::AMBER) &&
                   (msg.mode == LED_MODE::SOLID_HOT)) {
            _led_state.current_color = LED_COLOR::RED_AMBER;
            _led_state.current_mode = LED_MODE::PULSE;
        } else {
            _led_state.current_color = msg.color;
            _led_state.current_mode = msg.mode;
        }
        if ((_led_state.current_color == LED_COLOR::RED_WHITE) ||
            (_led_state.current_color == LED_COLOR::RED_AMBER) ||
            (_led_state.current_color == LED_COLOR::WHITE_AMBER)) {
            _led_state.led_alternate_colors = true;
            if (_led_state.current_color == LED_COLOR::RED_WHITE) {
                _led_state.led_color_1 = LED_COLOR::RED;
                _led_state.led_color_2 = LED_COLOR::WHITE;
            } else if (_led_state.current_color == LED_COLOR::RED_AMBER) {
                _led_state.led_color_1 = LED_COLOR::RED;
                _led_state.led_color_2 = LED_COLOR::AMBER;
            } else if (_led_state.current_color == LED_COLOR::WHITE_AMBER) {
                _led_state.led_color_1 = LED_COLOR::WHITE;
                _led_state.led_color_2 = LED_COLOR::AMBER;
            }
        } else {
            _led_state.led_alternate_colors = false;
        }
    }

    template <typename Policy>
    auto visit_message(const messages::UpdateLEDMessage& msg, Policy& policy)
        -> void {
        bool bStatus = true;
        if (!policy.check_I2C_ready()) {
            bStatus = false;
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::ErrorMessage{
                        .code = errors::ErrorCode::SYSTEM_LED_I2C_NOT_READY}));
        }
        if (bStatus) {
            if ((_led_state.current_mode == LED_MODE::SOLID_HOLDING) ||
                (_led_state.current_mode == LED_MODE::SOLID_HOT)) {
                if (policy.start_set_led(_led_state.current_color, 255) !=
                    errors::ErrorCode::NO_ERROR) {
                    bStatus = false;
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::ErrorMessage{
                                .code = errors::ErrorCode::
                                    SYSTEM_LED_TRANSMIT_ERROR}));
                }
            } else if (_led_state.current_mode == LED_MODE::PULSE) {
                uint8_t led_brightness_setting = 0;
                _led_state.led_tick_count += ONE;
                if (_led_state.led_tick_count > LED_TICKS_PER_PULSE) {
                    _led_state.led_tick_count = ONE;
                    _led_state.pulse_complete = !_led_state.pulse_complete;
                }
                if (_led_state.led_tick_count <= (LED_TICKS_PER_PULSE / TWO)) {
                    led_brightness_setting = static_cast<uint8_t>(
                        _led_state.led_tick_count *
                        (LED_FULL_SCALE / (LED_TICKS_PER_PULSE / TWO)));
                } else if ((_led_state.led_tick_count >
                            (LED_TICKS_PER_PULSE / TWO)) &&
                           (_led_state.led_tick_count <= LED_TICKS_PER_PULSE)) {
                    led_brightness_setting = static_cast<uint8_t>(
                        (LED_TICKS_PER_PULSE - _led_state.led_tick_count) *
                        (LED_FULL_SCALE / (LED_TICKS_PER_PULSE / TWO)));
                }
                LED_COLOR color_setting;
                if (_led_state.led_alternate_colors) {
                    if (!_led_state.pulse_complete) {
                        color_setting = _led_state.led_color_1;
                    } else {
                        color_setting = _led_state.led_color_2;
                    }
                } else {
                    color_setting = _led_state.current_color;
                }
                if (policy.start_set_led(color_setting,
                                         led_brightness_setting) !=
                    errors::ErrorCode::NO_ERROR) {
                    bStatus = false;
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::ErrorMessage{
                                .code = errors::ErrorCode::
                                    SYSTEM_LED_TRANSMIT_ERROR}));
                }
            } else if (_led_state.current_mode == LED_MODE::MODE_OFF) {
                if (policy.start_set_led(LED_COLOR::OFF, 255) !=
                    errors::ErrorCode::NO_ERROR) {
                    bStatus = false;
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            messages::ErrorMessage{
                                .code = errors::ErrorCode::
                                    SYSTEM_LED_TRANSMIT_ERROR}));
                }
            }
        }
    }

    template <typename Policy>
    auto visit_message(const messages::HandleLEDSetupError& msg, Policy& policy)
        -> void {
        auto error_message = messages::ErrorMessage{.code = msg.with_error};
        static_cast<void>(
            task_registry->comms->get_message_queue().try_send(error_message));
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
            get_message_queue().try_send(messages::UpdateLEDMessage{}));
    }

  private:
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    BootloaderPrepAckCache prep_cache;
    LEDState _led_state;
};

};  // namespace system_task
