/*
 * the primary interface to the host communications task
 */
#pragma once

#include <array>
#include <cstring>
#include <optional>
#include <utility>
#include <variant>

#include "core/ack_cache.hpp"
#include "core/gcode_parser.hpp"
#include "core/version.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/gcodes.hpp"
#include "hal/message_queue.hpp"

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
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;

  private:
    using GCodeParser = gcode::GroupParser<gcode::GetTMCRegister>;
    using AckOnlyCache =
        AckCache<8, gcode::EnterBootloader, gcode::SetSerialNumber>;
    using GetSystemInfoCache = AckCache<8, gcode::GetSystemInfo>;
    using GetTMCRegisterCache = AckCache<8, gcode::GetTMCRegister>;
  public:
    static constexpr size_t TICKS_TO_WAIT_ON_SEND = 10;
    explicit HostCommsTask(Queue& q, Aggregator* aggregator)
        : message_queue(q), task_registry(aggregator),
          ack_only_cache(),
          get_system_info_cache(), get_tmc_register_cache() {}
    HostCommsTask(const HostCommsTask& other) = delete;
    auto operator=(const HostCommsTask& other) -> HostCommsTask& = delete;
    HostCommsTask(HostCommsTask&& other) noexcept = delete;
    auto operator=(HostCommsTask&& other) noexcept -> HostCommsTask& = delete;
    ~HostCommsTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    auto provide_aggregator(Aggregator* aggregator) {
        task_registry = aggregator;
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
        static_cast<void>(msg);
        static_cast<void>(tx_into);
        static_cast<void>(tx_limit);
        return tx_into;
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::ForceUSBDisconnect& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        static_cast<void>(tx_limit);
        auto acknowledgement =
            messages::AcknowledgePrevious{.responding_to_id = response.id};
        may_connect_latch = false;
        static_cast<void>(task_registry->send_to_address(
            acknowledgement, response.return_address, TICKS_TO_WAIT_ON_SEND));
        return tx_into;
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::ErrorMessage& msg, InputIt tx_into,
                       InputLimit tx_limit) -> InputIt {
        return errors::write_into_async(tx_into, tx_limit, msg.code);
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
    auto visit_message(const messages::GetSystemInfoResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_system_info_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.serial_number,
                        response.fw_version, response.hw_version);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetTMCRegisterResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry = get_tmc_register_cache.remove_if_present(
            response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit);
                }
            },
            cache_entry);
    }

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
    auto visit_gcode(const gcode::GetTMCRegister& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_tmc_register_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetTMCRegisterMessage{.id = id, .motor_id = gcode.motor_id, .reg = gcode.reg};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_tmc_register_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    Queue& message_queue;
    Aggregator* task_registry;
    AckOnlyCache ack_only_cache;
    GetSystemInfoCache get_system_info_cache;
    GetTMCRegisterCache get_tmc_register_cache;
    bool may_connect_latch = true;
};

};  // namespace host_comms_task
