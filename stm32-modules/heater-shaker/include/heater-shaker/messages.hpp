#pragma once
#include <array>
#include <concepts>
#include <variant>

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
    { get_own_id(mt) }
    ->std::same_as<uint32_t>;
};

template <typename ResponseType>
concept Response = requires(ResponseType rt) {
    { get_responding_to_id(rt) }
    ->std::same_as<uint32_t>;
};

/*
** Message structs initiate actions. These may be changes in physical state, or
** a request to send back some data. Each carries an ID, which should be copied
*into
** the response.
*/
struct SetRPMMessage {
    uint32_t id;
    uint32_t target_rpm;
};

struct SetTemperatureMessage {
    uint32_t id;
    uint32_t target_temperature;
};

struct GetTemperatureMessage {
    uint32_t id;
};

struct GetRPMMessage {
    uint32_t id;
};

/*
** Response structs either confirm actions or fulfill actions. Because some
*messages
** don't have actual data associated with their response, AcknowledgePrevious is
*a
** generic response that carries only its responding_id and carries only the
** implication that the message has been received and acted upon. Responses like
** SetTemperatureResponse on the other hand serve to confirm that the action has
** been completed, and also carry the actual data requested.
*/
struct GetTemperatureResponse {
    uint32_t responding_to_id;
    uint32_t current_temperature;
    uint32_t setpoint_temperature;
};

struct GetRPMResponse {
    uint32_t responding_to_id;
    uint32_t current_rpm;
    uint32_t setpoint_rpm;
};

struct AcknowledgePrevious {
    uint32_t responding_to_id;
};

struct IncomingMessageFromHost {
    const char* buffer;
    size_t length;
};

using HeaterMessage = ::std::variant<std::monostate, SetTemperatureMessage,
                                     GetTemperatureMessage>;
using MotorMessage =
    ::std::variant<std::monostate, SetRPMMessage, GetRPMMessage>;
using UIMessage = ::std::variant<GetTemperatureResponse, GetRPMResponse>;
using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, AcknowledgePrevious,
                   GetTemperatureResponse, GetRPMResponse>;
};  // namespace messages
