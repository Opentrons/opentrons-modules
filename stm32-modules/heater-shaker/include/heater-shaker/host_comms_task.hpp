/*
 * the primary interface to the host communications task
 */
#pragma once

#include <array>
#include <cstring>
#include <optional>
#include <utility>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/ack_cache.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/gcode_parser.hpp"
#include "heater-shaker/gcodes.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater-shaker/version.hpp"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace host_comms_task {

using Message = messages::HostCommsMessage;

// By using a template template parameter here, we allow the code instantiating
// this template to do so as HostCommsTask<SomeQueueImpl> rather than
// HeaterTask<SomeQueueImpl<Message>>
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class HostCommsTask {
  public:
    using Queue = QueueImpl<Message>;

  private:
    using GCodeParser = gcode::GroupParser<
        gcode::SetRPM, gcode::SetTemperature, gcode::GetRPM,
        gcode::GetTemperature, gcode::SetAcceleration,
        gcode::GetTemperatureDebug, gcode::SetPIDConstants,
        gcode::SetHeaterPowerTest, gcode::EnterBootloader, gcode::GetVersion,
        gcode::Home, gcode::ActuateSolenoid, gcode::DebugControlPlateLockMotor>;
    using AckOnlyCache =
        AckCache<8, gcode::SetRPM, gcode::SetTemperature,
                 gcode::SetAcceleration, gcode::SetPIDConstants,
                 gcode::SetHeaterPowerTest, gcode::EnterBootloader, gcode::Home,
                 gcode::ActuateSolenoid, gcode::DebugControlPlateLockMotor>;
    using GetTempCache = AckCache<8, gcode::GetTemperature>;
    using GetTempDebugCache = AckCache<8, gcode::GetTemperatureDebug>;
    using GetRPMCache = AckCache<8, gcode::GetRPM>;

  public:
    static constexpr size_t TICKS_TO_WAIT_ON_SEND = 10;
    explicit HostCommsTask(Queue& q)
        : message_queue(q),
          task_registry(nullptr),
          // These nolints are because if you don't have these inits, host
          // builds complain NOLINTNEXTLINE(readability-redundant-member-init)
          ack_only_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_temp_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_rpm_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_temp_debug_cache() {}
    HostCommsTask(const HostCommsTask& other) = delete;
    auto operator=(const HostCommsTask& other) -> HostCommsTask& = delete;
    HostCommsTask(HostCommsTask&& other) noexcept = delete;
    auto operator=(HostCommsTask&& other) noexcept -> HostCommsTask& = delete;
    ~HostCommsTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

    /**
     * run_once() runs one spin of the task. This means it
     * - waits for a message to come in on its queue (either from another task,
     *or from the USB input handling machinery)
     * - handles the message
     *   - which may include sending other messages
     *   - which may include writing back a response string
     *
     * A buffer for the response string is provided by the caller, and it's
     *sadly provided as a c-style pointer+length pair because that's
     *fundamentally what it is. We could wrap it as an iterator pair, but it's
     *nice to be honest.
     *
     * This function returns the amount of data it actually wrote into tx_into.
     **/

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto run_once(InputIt tx_into, InputLimit tx_limit) -> InputLimit {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It may block
        // indefinitely
        message_queue.recv(&message);

        // We should now be guaranteed to have a message, and can visit it to do
        // our actual work.

        // we need a this-capturing lambda to pass on the call to our set of
        // member function overloads because otherwise we would need a pointer
        // to member function, and you can't really do that with variant visit.
        // we also need to current the transmit buffer details in.
        auto visit_helper = [this, tx_into,
                             tx_limit](auto& message) -> InputIt {
            return this->visit_message(message, tx_into, tx_limit);
        };

        // now, calling visit on the visit helper will pass through the calls to
        // our message handlers, and will pass through whatever the messages
        // return (aka how much data they wrote, if any) to the caller.
        return std::visit(visit_helper, message);
    }

    [[nodiscard]] auto may_connect() const -> bool { return may_connect_latch; }

  private:
    /**
     * visit_message is a set of overloads for all the messages that the task
     * accepts. Because of the way we're calling this, in a lambda with an auto
     * param passed to std::visit, the compiler ensures that we have a handler
     * for every kind of message we accept. All these functions have uniform
     * arguments (the particular message they handle and the tx buffer details)
     * and the same return type (how many bytes they put into the buffer, if
     * any). They may call other handler functions - for instance, the one that
     * handles incoming messages from usb does essentially this same pattern
     * again for whatever gcodes it parses.
     * */
    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::IncomingMessageFromHost& msg,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        // The parser is only really guaranteed to work if the message is
        // complete, ending in a newline, so let's make sure of that
        auto parser = GCodeParser();
        const auto* newline_at = std::find_if(
            msg.buffer, msg.limit,
            [](const auto& ch) { return ch == '\n' || ch == '\r'; });
        if (newline_at == msg.limit) {
            return tx_into;
        }
        // We're going to accumulate all the responses or errors we need into
        // our tx buffer if we can, so let's make some temps we're free to
        // modify.
        const auto* current = msg.buffer;
        InputIt current_tx_head = tx_into;
        // As in run_once, we're going to use std::visit to invoke a set of
        // member function overloads that also take other arguments, so we need
        // a lambda that closes over the extra arguments and curries them into
        // the member functions, as well as currying in the this pointer
        auto visit_helper = [this, &current_tx_head, &tx_limit](
                                auto gcode) -> std::pair<bool, InputIt> {
            return this->visit_gcode(gcode, current_tx_head, tx_limit);
        };
        while (true) {
            // Parse an incremental gcode
            auto maybe_parsed = parser.parse_available(current, msg.limit);
            current = maybe_parsed.second;
            // Visit it; this may write stuff to the transmit buffer, send
            // further messages, etc
            auto handled = std::visit(visit_helper, maybe_parsed.first);
            // Account for anything the handler might have written
            current_tx_head = handled.second;
            if (current_tx_head >= tx_limit) {
                // Something bad has happened, we overran or are about to
                // overrun our tx buffer, should let upstream know
                current_tx_head = errors::write_into(
                    tx_into, tx_limit, errors::ErrorCode::USB_TX_OVERRUN);

                break;
            }

            if ((!handled.first) || (current == msg.limit)) {
                // parse done
                break;
            }
        }
        return current_tx_head;
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::AcknowledgePrevious& msg,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            ack_only_cache.remove_if_present(msg.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, msg](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else if (msg.with_error != errors::ErrorCode::NO_ERROR) {
                    return errors::write_into(tx_into, tx_limit,
                                              msg.with_error);
                } else {
                    return cache_element.write_response_into(tx_into, tx_limit);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::ErrorMessage& msg, InputIt tx_into,
                       InputLimit tx_limit) -> InputIt {
        return errors::write_into(tx_into, tx_limit, msg.code);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const std::monostate& ignore, InputIt tx_into,
                       InputLimit tx_limit) -> InputIt {
        static_cast<void>(ignore);
        static_cast<void>(tx_into);
        static_cast<void>(tx_limit);
        return tx_into;
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetTemperatureResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_temp_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    if (response.with_error != errors::ErrorCode::NO_ERROR) {
                        return errors::write_into(tx_into, tx_limit,
                                                  response.with_error);
                    }
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.current_temperature,
                        response.setpoint_temperature);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetTemperatureDebugResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_temp_debug_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.pad_a_temperature,
                        response.pad_b_temperature, response.board_temperature,
                        response.pad_a_adc, response.pad_b_adc,
                        response.board_adc, response.power_good);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetRPMResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_rpm_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.current_rpm,
                        response.setpoint_rpm);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::ForceUSBDisconnectMessage& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        static_cast<void>(tx_limit);
        auto acknowledgement =
            messages::AcknowledgePrevious{.responding_to_id = response.id};
        may_connect_latch = false;
        static_cast<void>(task_registry->system->get_message_queue().try_send(
            acknowledgement));
        return tx_into;
    }

    /**
     * visit_gcode() is a set of member function overloads, each of which is
     * called when we parse the appropriate gcode out of the receive buffer.
     * They also get the transmit buffer details so they can write back data.
     * */

    // our parse-done handler does nothing
    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const std::monostate& ignore, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        static_cast<void>(ignore);
        static_cast<void>(tx_into);
        static_cast<void>(tx_limit);
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetVersion& ignore, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        static_cast<void>(ignore);
        auto written = gcode::GetVersion::write_response_into(
            tx_into, tx_limit, version::fw_version(), version::hw_version());
        return std::make_pair(true, written);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::ActuateSolenoid& solenoid_gcode,
                     InputIt tx_into, InputLimit tx_limit)
        -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(solenoid_gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::ActuateSolenoidMessage{
            .id = id, .current_ma = solenoid_gcode.current_ma};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::Home& home_code, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(home_code);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::BeginHomingMessage{.id = id};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetRPM& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetRPMMessage{.id = id, .target_rpm = gcode.rpm};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetAcceleration& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::SetAccelerationMessage{
            .id = id, .rpm_per_s = gcode.rpm_per_s};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetRPM& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_rpm_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetRPMMessage{.id = id};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_rpm_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetTemperature& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::SetTemperatureMessage{
            .id = id, .target_temperature = gcode.temperature};
        if (!task_registry->heater->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetTemperature& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_temp_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetTemperatureMessage{.id = id};
        if (!task_registry->heater->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_temp_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetTemperatureDebug& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_temp_debug_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetTemperatureDebugMessage{.id = id};
        if (!task_registry->heater->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_temp_debug_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetPIDConstants& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::SetPIDConstantsMessage{
            .id = id, .kp = gcode.kp, .ki = gcode.ki, .kd = gcode.kd};
        bool send_result = false;
        switch (gcode.target) {
            case gcode::SetPIDConstants::HEATER:
                send_result =
                    task_registry->heater->get_message_queue().try_send(
                        message, TICKS_TO_WAIT_ON_SEND);
                break;
            case gcode::SetPIDConstants::MOTOR:
                send_result =
                    task_registry->motor->get_message_queue().try_send(
                        message, TICKS_TO_WAIT_ON_SEND);
        }
        if (!send_result) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetHeaterPowerTest& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetPowerTestMessage{.id = id, .power = gcode.power};
        if (!task_registry->heater->get_message_queue().try_send(

                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::DebugControlPlateLockMotor& gcode,
                     InputIt tx_into, InputLimit tx_limit)
        -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetPlateLockPowerMessage{.id = id, .power = gcode.power};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::EnterBootloader& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::EnterBootloaderMessage{.id = id};
        if (!task_registry->system->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    // Our error handler just writes an error and bails
    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const GCodeParser::ParseError& _ignore, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        static_cast<void>(_ignore);
        return std::make_pair(
            false, errors::write_into(tx_into, tx_limit,
                                      errors::ErrorCode::UNHANDLED_GCODE));
    }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    AckOnlyCache ack_only_cache;
    GetTempCache get_temp_cache;
    GetRPMCache get_rpm_cache;
    GetTempDebugCache get_temp_debug_cache;
    bool may_connect_latch = true;
};

};  // namespace host_comms_task
