#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <variant>

#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"

namespace messages {

template <typename IdType, typename MessageType>
auto get_own_id(const MessageType& message) -> IdType {
    return message.id;
}

template <typename IdType, typename MessageType>
auto get_responding_to_id(const MessageType& message) -> IdType {
    return message.responding_to_id;
}

template <typename MessageType>
concept Message = requires(MessageType mt) {
    { get_own_id(mt) } -> std::same_as<uint32_t>;
};

template <typename ResponseType>
concept Response = requires(ResponseType rt) {
    { get_responding_to_id(rt) } -> std::same_as<uint32_t>;
};

/*
** Message structs initiate actions. These may be changes in physical state, or
** a request to send back some data. Each carries an ID, which should be copied
*into
** the response.
*/
// The from_system elements are a bit of a hack because we don't have full
// message source tracking and it seems weird to add it for literally two
// messages.
struct GetSystemInfoMessage {
    uint32_t id;
};

struct SetSerialNumberMessage {
    uint32_t id;
    static constexpr std::size_t SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SERIAL_NUMBER_LENGTH> serial_number;
};

struct EnterBootloaderMessage {
    uint32_t id;
};

struct ForceUSBDisconnectMessage {
    uint32_t id;
};

struct ErrorMessage {
    errors::ErrorCode code;
};

struct GetSystemInfoResponse {
    uint32_t responding_to_id;
    static constexpr std::size_t SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SERIAL_NUMBER_LENGTH> serial_number;
    const char* fw_version;
    const char* hw_version;
};

struct AcknowledgePrevious {
    uint32_t responding_to_id;
    errors::ErrorCode with_error = errors::ErrorCode::NO_ERROR;
};

struct IncomingMessageFromHost {
    const char* buffer;
    const char* limit;
};

struct ThermalPlateTempReadComplete {
    uint16_t heat_sink;
    uint16_t front_right;
    uint16_t front_center;
    uint16_t front_left;
    uint16_t back_right;
    uint16_t back_center;
    uint16_t back_left;
};

struct LidTempReadComplete {
    uint16_t lid_temp;
};

struct GetLidTemperatureDebugMessage {
    uint32_t id;
};

struct GetLidTemperatureDebugResponse {
    uint32_t responding_to_id;
    double lid_temp;
    uint16_t lid_adc;
};

struct GetPlateTemperatureDebugMessage {
    uint32_t id;
};

struct GetPlateTemperatureDebugResponse {
    uint32_t responding_to_id;
    double heat_sink_temp;
    double front_right_temp;
    double front_center_temp;
    double front_left_temp;
    double back_right_temp;
    double back_center_temp;
    double back_left_temp;

    uint16_t heat_sink_adc;
    uint16_t front_right_adc;
    uint16_t front_center_adc;
    uint16_t front_left_adc;
    uint16_t back_right_adc;
    uint16_t back_center_adc;
    uint16_t back_left_adc;
};

using SystemMessage =
    ::std::variant<std::monostate, EnterBootloaderMessage, AcknowledgePrevious,
                   SetSerialNumberMessage, GetSystemInfoMessage>;
using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, AcknowledgePrevious,
                   ErrorMessage, ForceUSBDisconnectMessage,
                   GetSystemInfoResponse, GetLidTemperatureDebugResponse,
                   GetPlateTemperatureDebugResponse>;
using ThermalPlateMessage =
    ::std::variant<std::monostate, ThermalPlateTempReadComplete,
                   GetPlateTemperatureDebugMessage>;
using LidHeaterMessage = ::std::variant<std::monostate, LidTempReadComplete,
                                        GetLidTemperatureDebugMessage>;
};  // namespace messages
