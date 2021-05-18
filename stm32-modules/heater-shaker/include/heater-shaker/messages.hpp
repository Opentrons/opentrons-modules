#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <variant>

#include "heater-shaker/errors.hpp"

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
    int16_t target_rpm;
};

struct SetTemperatureMessage {
    uint32_t id;
    double target_temperature;
};

struct GetTemperatureMessage {
    uint32_t id;
};

struct GetRPMMessage {
    uint32_t id;
};

struct SetAccelerationMessage {
    uint32_t id;
    int32_t rpm_per_s;
};

struct TemperatureConversionComplete {
    uint16_t pad_a;
    uint16_t pad_b;
    uint16_t board;
};

// Used internally to the motor task, communicates asynchronous errors to the
// main controller task
struct MotorSystemErrorMessage {
    uint16_t errors;
};

struct ErrorMessage {
    errors::ErrorCode code;
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
    double current_temperature;
    double setpoint_temperature;
    errors::ErrorCode with_error = errors::ErrorCode::NO_ERROR;
};

struct GetRPMResponse {
    uint32_t responding_to_id;
    int16_t current_rpm;
    int16_t setpoint_rpm;
};

struct AcknowledgePrevious {
    uint32_t responding_to_id;
    errors::ErrorCode with_error = errors::ErrorCode::NO_ERROR;
};

struct IncomingMessageFromHost {
    const char* buffer;
    const char* limit;
};

using HeaterMessage =
    ::std::variant<std::monostate, SetTemperatureMessage, GetTemperatureMessage,
                   TemperatureConversionComplete>;
using MotorMessage =
    ::std::variant<std::monostate, MotorSystemErrorMessage, SetRPMMessage,
                   GetRPMMessage, SetAccelerationMessage>;
using UIMessage = ::std::variant<GetTemperatureResponse, GetRPMResponse>;
using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, AcknowledgePrevious,
                   ErrorMessage, GetTemperatureResponse, GetRPMResponse>;
};  // namespace messages
