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
#include "flex-stacker/errors.hpp"
#include "flex-stacker/gcodes.hpp"
#include "flex-stacker/messages.hpp"
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
    using Queues = typename tasks::Tasks<QueueImpl>;

  private:
    using GCodeParser = gcode::GroupParser<
        gcode::GetTMCRegister, gcode::SetTMCRegister, gcode::SetRunCurrent,
        gcode::SetHoldCurrent, gcode::EnableMotor, gcode::DisableMotor,
        gcode::MoveMotorInSteps, gcode::MoveToLimitSwitch, gcode::MoveMotorInMm,
        gcode::GetLimitSwitches, gcode::SetMicrosteps, gcode::GetMoveParams>;
    using AckOnlyCache =
        AckCache<8, gcode::EnterBootloader, gcode::SetSerialNumber,
                 gcode::SetTMCRegister, gcode::SetRunCurrent,
                 gcode::SetHoldCurrent, gcode::EnableMotor, gcode::DisableMotor,
                 gcode::MoveMotorInSteps, gcode::MoveToLimitSwitch,
                 gcode::MoveMotorInMm, gcode::SetMicrosteps>;
    using GetSystemInfoCache = AckCache<8, gcode::GetSystemInfo>;
    using GetTMCRegisterCache = AckCache<8, gcode::GetTMCRegister>;
    using StallGuardCache = AckCache<8, gcode::StallGuardResult>;
    using GetLimitSwitchesCache = AckCache<8, gcode::GetLimitSwitches>;
    using GetMoveParamsCache = AckCache<8, gcode::GetMoveParams>;

  public:
    static constexpr size_t TICKS_TO_WAIT_ON_SEND = 10;
    explicit HostCommsTask(Queue& q, Aggregator* aggregator)
        : message_queue(q),
          task_registry(aggregator),
          // These nolints are because if you don't have these inits, host
          // builds complain NOLINTNEXTLINE(readability-redundant-member-init)
          ack_only_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_system_info_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_tmc_register_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_limit_switches_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_move_params_cache() {}
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
    auto visit_message(const messages::StallGuardResult& msg,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_element = gcode::StallGuardResult{};
        return cache_element.write_response_into(tx_into, tx_limit);
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
        auto cache_entry =
            get_tmc_register_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.motor_id, response.reg,
                        response.data);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetLimitSwitchesResponses& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry = get_limit_switches_cache.remove_if_present(
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
                        tx_into, tx_limit, response.x_extend_triggered,
                        response.x_retract_triggered,
                        response.z_extend_triggered,
                        response.x_retract_triggered,
                        response.l_released_triggered,
                        response.l_held_triggered);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetMoveParamsResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_move_params_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.motor_id, response.velocity,
                        response.acceleration, response.velocity_discont);
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
        auto message = messages::GetTMCRegisterMessage{
            .id = id, .motor_id = gcode.motor_id, .reg = gcode.reg};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_tmc_register_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetTMCRegister& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetTMCRegisterMessage{.id = id,
                                            .motor_id = gcode.motor_id,
                                            .reg = gcode.reg,
                                            .data = gcode.data};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::SetRunCurrent& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetMotorCurrentMessage{.id = id,
                                             .motor_id = gcode.motor_id,
                                             .run_current = gcode.current,
                                             .hold_current = 0.0};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::SetHoldCurrent& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetMotorCurrentMessage{.id = id,
                                             .motor_id = gcode.motor_id,
                                             .run_current = 0.0,
                                             .hold_current = gcode.current};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::SetMicrosteps& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::SetMicrostepsMessage{
            .id = id,
            .motor_id = gcode.motor_id,
            .microsteps_power =
                std::min(gcode.microsteps_power, static_cast<uint8_t>(8))};
        if (!task_registry->send_to_address(message, Queues::MotorDriverAddress,
                                            TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::EnableMotor& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::MotorEnableMessage{
            .id = id, .x = gcode.x, .z = gcode.z, .l = gcode.l};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::DisableMotor& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::MotorEnableMessage{
            .id = id, .x = gcode.x, .z = gcode.z, .l = gcode.l};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::MoveMotorInSteps& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::MoveMotorInStepsMessage{
            .id = id,
            .motor_id = gcode.motor_id,
            .steps = gcode.steps,
            .steps_per_second = gcode.steps_per_second,
            .steps_per_second_sq = gcode.steps_per_second_sq};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::MoveMotorInMm& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::MoveMotorInMmMessage{
            .id = id,
            .motor_id = gcode.motor_id,
            .mm = gcode.mm,
            .mm_per_second = gcode.mm_per_second,
            .mm_per_second_sq = gcode.mm_per_second_sq,
            .mm_per_second_discont = gcode.mm_per_second_discont};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::MoveToLimitSwitch& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::MoveToLimitSwitchMessage{
            .id = id,
            .motor_id = gcode.motor_id,
            .direction = gcode.direction,
            .mm_per_second = gcode.mm_per_second,
            .mm_per_second_sq = gcode.mm_per_second_sq,
            .mm_per_second_discont = gcode.mm_per_second_discont};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::MoveMotor& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::MoveMotorMessage{.id = id,
                                                  .motor_id = gcode.motor_id,
                                                  .direction = gcode.direction,
                                                  .frequency = gcode.frequency};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::StopMotor& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::StopMotorMessage{.id = id};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
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
    auto visit_gcode(const gcode::GetLimitSwitches& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_limit_switches_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetLimitSwitchesMessage{.id = id};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_limit_switches_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetMoveParams& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_move_params_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetMoveParamsMessage{
            .id = id, .motor_id = gcode.motor_id};
        if (!task_registry->send(message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_move_params_cache.remove_if_present(id);
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
    Aggregator* task_registry;
    AckOnlyCache ack_only_cache;
    GetSystemInfoCache get_system_info_cache;
    GetTMCRegisterCache get_tmc_register_cache;
    GetLimitSwitchesCache get_limit_switches_cache;
    GetMoveParamsCache get_move_params_cache;
    bool may_connect_latch = true;
};

};  // namespace host_comms_task
