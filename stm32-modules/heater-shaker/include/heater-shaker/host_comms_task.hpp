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
requires MessageQueue<QueueImpl<Message>, Message> class HostCommsTask {
    using Queue = QueueImpl<Message>;
    using GCodeParser =
        gcode::GroupParser<gcode::SetRPM, gcode::SetTemperature, gcode::GetRPM,
                           gcode::GetTemperature>;
    using AckOnlyCache = AckCache<8, gcode::SetRPM, gcode::SetTemperature>;
    using GetTempCache = AckCache<8, gcode::GetTemperature>;
    using GetRPMCache = AckCache<8, gcode::GetRPM>;

  public:
    static constexpr size_t TICKS_TO_WAIT_ON_SEND = 10;
    explicit HostCommsTask(Queue& q)
        : message_queue(q),
          task_registry(nullptr),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          ack_only_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_temp_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_rpm_cache() {}
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
    auto run_once(char* tx_into, size_t tx_length_available) -> size_t {
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
                             tx_length_available](auto& message) -> size_t {
            return this->visit_message(message, tx_into, tx_length_available);
        };

        // now, calling visit on the visit helper will pass through the calls to
        // our message handlers, and will pass through whatever the messages
        // return (aka how much data they wrote, if any) to the caller.
        return std::visit(visit_helper, message);
    }

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
    auto visit_message(const messages::IncomingMessageFromHost& msg,
                       char* tx_into, size_t tx_length_available) -> size_t {
        // The parser is only really guaranteed to work if the message is
        // complete, ending in a newline, so let's make sure of that
        auto parser = GCodeParser();
        const auto* newline_at = std::find_if(
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            msg.buffer, msg.buffer + msg.length,
            [](const auto& ch) { return ch == '\n' || ch == '\r'; });
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (newline_at == (msg.buffer + msg.length)) {
            return 0;
        }
        // We're going to accumulate all the responses or errors we need into
        // our tx buffer if we can, so let's make some temps we're free to
        // modify.
        const auto* current = msg.buffer;
        size_t written = 0;
        size_t current_available_length = tx_length_available;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        char* current_tx_head = tx_into;
        // As in run_once, we're going to use std::visit to invoke a set of
        // member function overloads that also take other arguments, so we need
        // a lambda that closes over the extra arguments and curries them into
        // the member functions, as well as currying in the this pointer
        auto visit_helper = [this, &current_tx_head, &current_available_length](
                                auto gcode) -> std::pair<bool, size_t> {
            return this->visit_gcode(gcode, current_tx_head,
                                     current_available_length);
        };
        while (true) {
            // Parse an incremental gcode
            auto maybe_parsed =
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                parser.parse_available(current, msg.buffer + msg.length);
            current = maybe_parsed.second;
            // Visit it; this may write stuff to the transmit buffer, send
            // further messages, etc
            auto handled = std::visit(visit_helper, maybe_parsed.first);
            // Account for anything the handler might have written
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            current_available_length -= handled.second;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            current_tx_head += handled.second;
            written += handled.second;

            if (written >= current_available_length) {
                // Something bad has happened, we overran or are about to
                // overrun our tx buffer, should let upstream know
                if (sizeof(errors::USBTXBufOverrun::errorstring()) <=
                    tx_length_available) {
                    errors::write_into<errors::USBTXBufOverrun>(
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        tx_into + tx_length_available -
                            strlen(errors::USBTXBufOverrun::errorstring()),
                        strlen(errors::USBTXBufOverrun::errorstring()));
                }
                written = tx_length_available;
                break;
            }

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if ((!handled.first) || (current == msg.buffer + msg.length)) {
                // parse done
                break;
            }
        }
        return written;
    }

    // NOLINTNEXTLINE(readability-non-const-parameter)
    auto visit_message(const messages::AcknowledgePrevious& msg, char* tx_into,
                       size_t tx_length_available) -> size_t {
        auto cache_entry =
            ack_only_cache.remove_if_present(msg.responding_to_id);
        return std::visit(
            [tx_into, tx_length_available](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into<
                        errors::BadMessageAcknowledgement>(tx_into,
                                                           tx_length_available);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_length_available);
                }
            },
            cache_entry);
    }
    // NOLINTNEXTLINE(readability-non-const-parameter)
    auto visit_message(const std::monostate& ignore, char* tx_into,
                       size_t tx_length_available) -> size_t {
        static_cast<void>(ignore);
        static_cast<void>(tx_into);
        static_cast<void>(tx_length_available);
        return 0;
    }

    auto visit_message(const messages::GetTemperatureResponse& response,
                       // NOLINTNEXTLINE(readability-non-const-parameter)
                       char* tx_into, size_t tx_length_available) -> size_t {
        auto cache_entry =
            get_temp_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_length_available, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into<
                        errors::BadMessageAcknowledgement>(tx_into,
                                                           tx_length_available);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_length_available,
                        response.current_temperature,
                        response.setpoint_temperature);
                }
            },
            cache_entry);
    }

    // NOLINTNEXTLINE(readability-non-const-parameter)
    auto visit_message(const messages::GetRPMResponse& response, char* tx_into,
                       size_t tx_length_available) -> size_t {
        auto cache_entry =
            get_rpm_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_length_available, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into<
                        errors::BadMessageAcknowledgement>(tx_into,
                                                           tx_length_available);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_length_available, response.current_rpm,
                        response.setpoint_rpm);
                }
            },
            cache_entry);
    }

    /**
     * visit_gcode() is a set of member function overloads, each of which is
     * called when we parse the appropriate gcode out of the receive buffer.
     * They also get the transmit buffer details so they can write back data.
     * */

    // our parse-done handler does nothing
    // NOLINTNEXTLINE(readability-non-const-parameter)
    auto visit_gcode(const std::monostate& ignore, char* tx_into,
                     size_t tx_length_available) -> std::pair<bool, size_t> {
        static_cast<void>(ignore);
        static_cast<void>(tx_into);
        static_cast<void>(tx_length_available);
        return std::make_pair(true, 0);
    }

    auto visit_gcode(const gcode::SetRPM& gcode, char* tx_into,
                     size_t tx_length_available) -> std::pair<bool, size_t> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(false,
                                  errors::write_into<errors::GCodeCacheFull>(
                                      tx_into, tx_length_available));
        }
        auto message =
            messages::SetRPMMessage{.id = id, .target_rpm = gcode.rpm};
        if (!task_registry->motor.get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto written = errors::write_into<errors::InternalQueueFull>(
                tx_into, tx_length_available);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, written);
        }
        return std::make_pair(true, 0);
    }

    auto visit_gcode(const gcode::GetRPM& gcode, char* tx_into,
                     size_t tx_length_available) -> std::pair<bool, size_t> {
        auto id = get_rpm_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(false,
                                  errors::write_into<errors::GCodeCacheFull>(
                                      tx_into, tx_length_available));
        }
        auto message = messages::GetRPMMessage{.id = id};
        if (!task_registry->motor.get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto written = errors::write_into<errors::InternalQueueFull>(
                tx_into, tx_length_available);
            get_rpm_cache.remove_if_present(id);
            return std::make_pair(false, written);
        }
        return std::make_pair(true, 0);
    }

    auto visit_gcode(const gcode::SetTemperature& gcode, char* tx_into,
                     size_t tx_length_available) -> std::pair<bool, size_t> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(false,
                                  errors::write_into<errors::GCodeCacheFull>(
                                      tx_into, tx_length_available));
        }
        auto message = messages::SetTemperatureMessage{
            .id = id,
            .target_temperature = static_cast<uint32_t>(gcode.temperature)};
        if (!task_registry->heater.get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto written = errors::write_into<errors::InternalQueueFull>(
                tx_into, tx_length_available);
            ack_only_cache.remove_if_present(id);
            return std::make_pair(false, written);
        }
        return std::make_pair(true, 0);
    }

    auto visit_gcode(const gcode::GetTemperature& gcode, char* tx_into,
                     size_t tx_length_available) -> std::pair<bool, size_t> {
        auto id = get_temp_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(false,
                                  errors::write_into<errors::GCodeCacheFull>(
                                      tx_into, tx_length_available));
        }
        auto message = messages::GetTemperatureMessage{.id = id};
        if (!task_registry->heater.get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto written = errors::write_into<errors::InternalQueueFull>(
                tx_into, tx_length_available);
            get_temp_cache.remove_if_present(id);
            return std::make_pair(false, written);
        }
        return std::make_pair(true, 0);
    }

    // Our error handler just writes an error and bails
    auto visit_gcode(const GCodeParser::ParseError& _ignore, char* tx_into,
                     size_t tx_length_available) -> std::pair<bool, size_t> {
        static_cast<void>(_ignore);
        return std::make_pair(false, errors::write_into<errors::UnhandledGCode>(
                                         tx_into, tx_length_available));
    }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    AckOnlyCache ack_only_cache;
    GetTempCache get_temp_cache;
    GetRPMCache get_rpm_cache;
};

};  // namespace host_comms_task
