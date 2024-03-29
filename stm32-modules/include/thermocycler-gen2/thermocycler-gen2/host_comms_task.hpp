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
#include "hal/message_queue.hpp"
#include "thermocycler-gen2/board_revision.hpp"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/gcodes.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/tasks.hpp"

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
        gcode::EnterBootloader, gcode::GetSystemInfo, gcode::SetSerialNumber,
        gcode::GetLidTemperatureDebug, gcode::GetPlateTemperatureDebug,
        gcode::ActuateSolenoid, gcode::ActuateLidStepperDebug,
        gcode::SetPeltierDebug, gcode::SetFanManual, gcode::SetHeaterDebug,
        gcode::GetPlateTemp, gcode::GetLidTemp, gcode::SetLidTemperature,
        gcode::DeactivateLidHeating, gcode::SetPIDConstants,
        gcode::SetPlateTemperature, gcode::DeactivatePlate,
        gcode::SetFanAutomatic, gcode::ActuateSealStepperDebug,
        gcode::GetSealDriveStatus, gcode::SetSealParameter, gcode::GetLidStatus,
        gcode::GetThermalPowerDebug, gcode::SetOffsetConstants,
        gcode::GetOffsetConstants, gcode::OpenLid, gcode::CloseLid,
        gcode::LiftPlate, gcode::DeactivateAll, gcode::GetBoardRevision,
        gcode::GetLidSwitches, gcode::GetFrontButton, gcode::SetLidFans,
        gcode::SetLightsDebug>;
    using AckOnlyCache =
        AckCache<8, gcode::EnterBootloader, gcode::SetSerialNumber,
                 gcode::ActuateSolenoid, gcode::ActuateLidStepperDebug,
                 gcode::SetPeltierDebug, gcode::SetFanManual,
                 gcode::SetHeaterDebug, gcode::SetLidTemperature,
                 gcode::DeactivateLidHeating, gcode::SetPIDConstants,
                 gcode::SetPlateTemperature, gcode::DeactivatePlate,
                 gcode::SetFanAutomatic, gcode::SetSealParameter,
                 gcode::SetOffsetConstants, gcode::OpenLid, gcode::CloseLid,
                 gcode::LiftPlate, gcode::SetLidFans, gcode::SetLightsDebug>;
    using GetSystemInfoCache = AckCache<8, gcode::GetSystemInfo>;
    using GetLidTempDebugCache = AckCache<8, gcode::GetLidTemperatureDebug>;
    using GetPlateTempDebugCache = AckCache<8, gcode::GetPlateTemperatureDebug>;
    using GetPlateTempCache = AckCache<8, gcode::GetPlateTemp>;
    using GetLidTempCache = AckCache<8, gcode::GetLidTemp>;
    using GetSealDriveStatusCache = AckCache<8, gcode::GetSealDriveStatus>;
    using GetLidStatusCache = AckCache<8, gcode::GetLidStatus>;
    using GetOffsetConstantsCache = AckCache<8, gcode::GetOffsetConstants>;
    using SealStepperDebugCache = AckCache<8, gcode::ActuateSealStepperDebug>;
    // This is a two-stage message since both the Plate and Lid tasks have
    // to respond.
    using GetThermalPowerCache = AckCache<8, gcode::GetThermalPowerDebug,
                                          messages::GetPlatePowerResponse>;
    // This is a two-stage message since both the Plate and Lid tasks have
    // to respond.
    using DeactivateAllCache =
        AckCache<8, gcode::DeactivateAll, messages::DeactivateAllResponse>;
    // Shared cache for debugging commands intended for In Circuit Test Fixture
    using GetSwitchCache =
        AckCache<8, gcode::GetLidSwitches, gcode::GetFrontButton>;

  public:
    static constexpr size_t TICKS_TO_WAIT_ON_SEND = 10;
    explicit HostCommsTask(Queue& q)
        : message_queue(q),
          task_registry(nullptr),
          // These nolints are because if you don't have these inits, host
          // builds complain NOLINTNEXTLINE(readability-redundant-member-init)
          ack_only_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_system_info_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_lid_temp_debug_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_plate_temp_debug_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_plate_temp_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_lid_temp_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_seal_drive_status_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_lid_status_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_offset_constants_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          seal_stepper_debug_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_thermal_power_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          deactivate_all_cache(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          get_switch_cache() {}
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

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(
        const messages::GetPlateTemperatureDebugResponse& response,
        InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry = get_plate_temp_debug_cache.remove_if_present(
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
                        tx_into, tx_limit, response.heat_sink_temp,
                        response.front_right_temp, response.front_left_temp,
                        response.front_center_temp, response.back_right_temp,
                        response.back_left_temp, response.back_center_temp,
                        response.heat_sink_adc, response.front_right_adc,
                        response.front_left_adc, response.front_center_adc,
                        response.back_right_adc, response.back_left_adc,
                        response.back_center_adc);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetLidTemperatureDebugResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry = get_lid_temp_debug_cache.remove_if_present(
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
                        tx_into, tx_limit, response.lid_temp, response.lid_adc);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetLidTempResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_lid_temp_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.current_temp,
                        response.set_temp);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetPlateTempResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_plate_temp_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.current_temp,
                        response.set_temp, response.time_remaining,
                        response.total_time, response.at_target);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetSealDriveStatusResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry = get_seal_drive_status_cache.remove_if_present(
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
                        tx_into, tx_limit, response.status, response.tstep);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetLidStatusResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_lid_status_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.lid, response.seal);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetOffsetConstantsResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        // Now we can send complete response to host computer
        auto cache_entry = get_offset_constants_cache.remove_if_present(
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
                        tx_into, tx_limit, response.a, response.bl, response.cl,
                        response.bc, response.cc, response.br, response.cr);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetPlatePowerResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        // Follow up by sending a message to the lid
        auto cache_entry = get_thermal_power_cache.remove_if_present(
            response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response, this](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (!std::is_same_v<gcode::GetThermalPowerDebug, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    auto id = get_thermal_power_cache.add(response);
                    if (id == 0) {
                        return errors::write_into(
                            tx_into, tx_limit,
                            errors::ErrorCode::GCODE_CACHE_FULL);
                    }
                    auto message = messages::GetThermalPowerMessage{.id = id};
                    if (!task_registry->lid_heater->get_message_queue()
                             .try_send(message, TICKS_TO_WAIT_ON_SEND)) {
                        get_thermal_power_cache.remove_if_present(id);
                        return errors::write_into(
                            tx_into, tx_limit,
                            errors::ErrorCode::INTERNAL_QUEUE_FULL);
                    }
                    // Nothing gets written for this command
                    return tx_into;
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetLidPowerResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        // Now we can send complete response to host computer
        auto cache_entry = get_thermal_power_cache.remove_if_present(
            response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (!std::is_same_v<messages::GetPlatePowerResponse,
                                              T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return gcode::GetThermalPowerDebug::write_response_into(
                        tx_into, tx_limit, cache_element.left,
                        cache_element.center, cache_element.right,
                        response.heater, cache_element.fans,
                        cache_element.tach1, cache_element.tach2);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::SealStepperDebugResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry = seal_stepper_debug_cache.remove_if_present(
            response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    if (response.with_error == errors::ErrorCode::NO_ERROR) {
                        return cache_element.write_response_into(
                            tx_into, tx_limit, response.steps_taken);
                    }
                    return errors::write_into(tx_into, tx_limit,
                                              response.with_error);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::DeactivateAllResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        // Follow up by sending a message to the lid
        auto cache_entry =
            deactivate_all_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response, this](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<gcode::DeactivateAll, T>) {
                    // This is the first response, from the plate task.
                    // Now send a message to the Lid task.
                    auto id = deactivate_all_cache.add(response);
                    if (id == 0) {
                        return errors::write_into(
                            tx_into, tx_limit,
                            errors::ErrorCode::GCODE_CACHE_FULL);
                    }
                    auto message = messages::DeactivateAllMessage{.id = id};
                    if (!task_registry->lid_heater->get_message_queue()
                             .try_send(message, TICKS_TO_WAIT_ON_SEND)) {
                        deactivate_all_cache.remove_if_present(id);
                        return errors::write_into(
                            tx_into, tx_limit,
                            errors::ErrorCode::INTERNAL_QUEUE_FULL);
                    }
                    // Nothing gets written for this command
                    return tx_into;
                } else if constexpr (std::is_same_v<
                                         messages::DeactivateAllResponse, T>) {
                    // This is the second response. Write the final response
                    // now.
                    return gcode::DeactivateAll::write_response_into(tx_into,
                                                                     tx_limit);
                } else {
                    // Could not find this item
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetLidSwitchesResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_switch_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (!std::is_same_v<gcode::GetLidSwitches, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.close_switch_pressed,
                        response.open_switch_pressed,
                        response.seal_extension_pressed,
                        response.seal_retraction_pressed);
                }
            },
            cache_entry);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_message(const messages::GetFrontButtonResponse& response,
                       InputIt tx_into, InputLimit tx_limit) -> InputIt {
        auto cache_entry =
            get_switch_cache.remove_if_present(response.responding_to_id);
        return std::visit(
            [tx_into, tx_limit, response](auto cache_element) {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (!std::is_same_v<gcode::GetFrontButton, T>) {
                    return errors::write_into(
                        tx_into, tx_limit,
                        errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT);
                } else {
                    return cache_element.write_response_into(
                        tx_into, tx_limit, response.button_pressed);
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
    auto visit_gcode(const gcode::GetSystemInfo& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_system_info_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetSystemInfoMessage{.id = id};
        if (!task_registry->system->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_system_info_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetSerialNumber& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        if (gcode.with_error ==
            errors::ErrorCode::SYSTEM_SERIAL_NUMBER_INVALID) {
            return std::make_pair(
                false, errors::write_into(
                           tx_into, tx_limit,
                           errors::ErrorCode::SYSTEM_SERIAL_NUMBER_INVALID));
        }
        auto message = messages::SetSerialNumberMessage{
            .id = id, .serial_number = gcode.serial_number};
        if (!task_registry->system->get_message_queue().try_send(
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

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetLidTemperatureDebug& gcode,
                     InputIt tx_into, InputLimit tx_limit)
        -> std::pair<bool, InputIt> {
        auto id = get_lid_temp_debug_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::GetLidTemperatureDebugMessage{.id = id};
        if (!task_registry->lid_heater->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_lid_temp_debug_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetLidTemp& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_lid_temp_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::GetLidTempMessage{.id = id};
        if (!task_registry->lid_heater->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_lid_temp_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetPlateTemperatureDebug& gcode,
                     InputIt tx_into, InputLimit tx_limit)
        -> std::pair<bool, InputIt> {
        auto id = get_plate_temp_debug_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::GetPlateTemperatureDebugMessage{.id = id};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_plate_temp_debug_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
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
            .id = id, .engage = solenoid_gcode.engage};
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
    auto visit_gcode(const gcode::GetPlateTemp& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_plate_temp_cache.add(gcode);
        auto message = messages::GetPlateTempMessage{.id = id};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_plate_temp_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetPeltierDebug& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::SetPeltierDebugMessage{
            .id = id,
            .power = gcode.power,
            .direction = gcode.direction,
            .selection = gcode.peltier_selection};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::SetFanManual& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message =
            messages::SetFanManualMessage{.id = id, .power = gcode.power};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::SetFanAutomatic& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::SetFanAutomaticMessage{.id = id};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::ActuateSealStepperDebug& gcode,
                     InputIt tx_into, InputLimit tx_limit)
        -> std::pair<bool, InputIt> {
        auto id = seal_stepper_debug_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::SealStepperDebugMessage{
            .id = id, .steps = gcode.distance};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            seal_stepper_debug_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }

        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetHeaterDebug& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message =
            messages::SetHeaterDebugMessage{.id = id, .power = gcode.power};
        if (!task_registry->lid_heater->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::SetLidTemperature& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::SetLidTemperatureMessage{
            .id = id, .setpoint = gcode.setpoint};
        if (!task_registry->lid_heater->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::DeactivateLidHeating& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::DeactivateLidHeatingMessage{.id = id};
        if (!task_registry->lid_heater->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::SetPIDConstants& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message =
            messages::SetPIDConstantsMessage{.id = id,
                                             .selection = gcode.selection,
                                             .p = gcode.const_p,
                                             .i = gcode.const_i,
                                             .d = gcode.const_d};
        bool ret = false;
        if (message.selection == PidSelection::HEATER) {
            ret = task_registry->lid_heater->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND);
        } else {
            ret = task_registry->thermal_plate->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND);
        }
        if (!ret) {
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
    auto visit_gcode(const gcode::SetPlateTemperature& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message =
            messages::SetPlateTemperatureMessage{.id = id,
                                                 .setpoint = gcode.setpoint,
                                                 .hold_time = gcode.hold_time,
                                                 .volume = gcode.volume};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::DeactivatePlate& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }

        auto message = messages::DeactivatePlateMessage{.id = id};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::ActuateLidStepperDebug& lid_stepper_gcode,
                     InputIt tx_into, InputLimit tx_limit)
        -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(lid_stepper_gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::LidStepperDebugMessage{
            .id = id,
            .angle = lid_stepper_gcode.angle,
            .overdrive = lid_stepper_gcode.overdrive};
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
    auto visit_gcode(const gcode::GetSealDriveStatus& seal_drive_gcode,
                     InputIt tx_into, InputLimit tx_limit)
        -> std::pair<bool, InputIt> {
        auto id = get_seal_drive_status_cache.add(seal_drive_gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetSealDriveStatusMessage{.id = id};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_seal_drive_status_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetSealParameter& seal_gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(seal_gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::SetSealParameterMessage{
            .id = id, .param = seal_gcode.parameter, .value = seal_gcode.value};
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
    auto visit_gcode(const gcode::GetLidStatus& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_lid_status_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetLidStatusMessage{.id = id};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_lid_status_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetThermalPowerDebug& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_thermal_power_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetThermalPowerMessage{.id = id};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_thermal_power_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetOffsetConstants& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetOffsetConstantsMessage{.id = id,
                                                .channel = gcode.channel,
                                                .a_set = gcode.const_a.defined,
                                                .const_a = gcode.const_a.value,
                                                .b_set = gcode.const_b.defined,
                                                .const_b = gcode.const_b.value,
                                                .c_set = gcode.const_c.defined,
                                                .const_c = gcode.const_c.value};

        if (!task_registry->thermal_plate->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::GetOffsetConstants& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_offset_constants_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetOffsetConstantsMessage{.id = id};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_offset_constants_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::CloseLid& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::CloseLidMessage{.id = id};
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
    auto visit_gcode(const gcode::OpenLid& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::OpenLidMessage{.id = id};
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
    auto visit_gcode(const gcode::LiftPlate& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::PlateLiftMessage{.id = id};
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
    auto visit_gcode(const gcode::DeactivateAll& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = deactivate_all_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::DeactivateAllMessage{.id = id};
        if (!task_registry->thermal_plate->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            deactivate_all_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetBoardRevision& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        // The board revision driver is already populated, so we can actually
        // send the response immediately!
        auto revision = board_revision::BoardRevisionIface::get();
        auto wrote_to = gcode.write_response_into(tx_into, tx_limit,
                                                  static_cast<int>(revision));
        return std::make_pair(true, wrote_to);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetLidSwitches& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_switch_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetLidSwitchesMessage{.id = id};
        if (!task_registry->motor->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_switch_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::GetFrontButton& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = get_switch_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message = messages::GetFrontButtonMessage{.id = id};
        if (!task_registry->system->get_message_queue().try_send(
                message, TICKS_TO_WAIT_ON_SEND)) {
            auto wrote_to = errors::write_into(
                tx_into, tx_limit, errors::ErrorCode::INTERNAL_QUEUE_FULL);
            get_switch_cache.remove_if_present(id);
            return std::make_pair(false, wrote_to);
        }
        return std::make_pair(true, tx_into);
    }

    template <typename InputIt, typename InputLimit>
    requires std::forward_iterator<InputIt> &&
        std::sized_sentinel_for<InputLimit, InputIt>
    auto visit_gcode(const gcode::SetLidFans& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetLidFansMessage{.id = id, .enable = gcode.enable};
        if (!task_registry->lid_heater->get_message_queue().try_send(
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
    auto visit_gcode(const gcode::SetLightsDebug& gcode, InputIt tx_into,
                     InputLimit tx_limit) -> std::pair<bool, InputIt> {
        auto id = ack_only_cache.add(gcode);
        if (id == 0) {
            return std::make_pair(
                false, errors::write_into(tx_into, tx_limit,
                                          errors::ErrorCode::GCODE_CACHE_FULL));
        }
        auto message =
            messages::SetLightsDebugMessage{.id = id, .enable = gcode.enable};
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
    GetSystemInfoCache get_system_info_cache;
    GetLidTempDebugCache get_lid_temp_debug_cache;
    GetPlateTempDebugCache get_plate_temp_debug_cache;
    GetPlateTempCache get_plate_temp_cache;
    GetLidTempCache get_lid_temp_cache;
    GetSealDriveStatusCache get_seal_drive_status_cache;
    GetLidStatusCache get_lid_status_cache;
    GetOffsetConstantsCache get_offset_constants_cache;
    SealStepperDebugCache seal_stepper_debug_cache;
    GetThermalPowerCache get_thermal_power_cache;
    DeactivateAllCache deactivate_all_cache;
    GetSwitchCache get_switch_cache;
    bool may_connect_latch = true;
};

};  // namespace host_comms_task
