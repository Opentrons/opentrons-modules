#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <variant>

#include "systemwide.hpp"
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
        systemwide::SERIAL_NUMBER_LENGTH;
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
        systemwide::SERIAL_NUMBER_LENGTH;
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

using SystemMessage =
    ::std::variant<std::monostate, EnterBootloaderMessage, AcknowledgePrevious,
                   SetSerialNumberMessage, GetSystemInfoMessage>;
using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, AcknowledgePrevious,
                   ErrorMessage, ForceUSBDisconnectMessage,
                   GetSystemInfoResponse>;
using ThermalPlateMessage =
    ::std::variant<std::monostate, ThermalPlateTempReadComplete>;
using LidHeaterMessage = ::std::variant<std::monostate, LidTempReadComplete>;
};  // namespace messages
