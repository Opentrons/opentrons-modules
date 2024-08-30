#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <variant>

#include "flex-stacker/errors.hpp"
#include "systemwide.h"

namespace messages {

template <typename IdType, typename MessageType>
auto get_own_id(const MessageType& message) -> IdType {
    return message.id;
}

template <typename IdType, typename MessageType>
auto get_responding_to_id(const MessageType& message) -> IdType {
    return message.responding_to_id;
}

template <typename AddrType, typename MessageType>
auto get_return_address(const MessageType& message) -> AddrType {
    return message.return_address;
}

template <typename MessageType>
concept Message = requires(MessageType mt) {
    { get_own_id(mt) } -> std::same_as<uint32_t>;
};

template <typename MessageType>
concept MessageWithReturn = requires(MessageType mt) {
    { get_return_address(mt) } -> std::same_as<size_t>;
}
&&Message<MessageType>;

template <typename ResponseType>
concept Response = requires(ResponseType rt) {
    { get_responding_to_id(rt) } -> std::same_as<uint32_t>;
};

/*
** Message structs initiate actions. These may be changes in physical state, or
** a request to send back some data. Each carries an ID, which should be copied
** into the response.
*/

struct ErrorMessage {
    errors::ErrorCode code;
};

struct AcknowledgePrevious {
    uint32_t responding_to_id{};
    errors::ErrorCode with_error = errors::ErrorCode::NO_ERROR;
};

struct IncomingMessageFromHost {
    const char* buffer;
    const char* limit;
};

struct GetSystemInfoMessage {
    uint32_t id;
};

struct GetSystemInfoResponse {
    uint32_t responding_to_id;
    static constexpr std::size_t SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SERIAL_NUMBER_LENGTH> serial_number;
    const char* fw_version;
    const char* hw_version;
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

struct ForceUSBDisconnect {
    uint32_t id;
    size_t return_address;
};

struct SetTMCRegisterMessage {
    uint32_t id;
    MotorID motor_id;
    uint8_t reg;
    uint32_t data;
};

struct GetTMCRegisterMessage {
    uint32_t id;
    MotorID motor_id;
    uint8_t reg;
};

struct PollTMCRegisterMessage {
    uint32_t id;
    MotorID motor_id;
    uint8_t reg;
};

struct StopPollTMCRegisterMessage {
    uint32_t id;
};

struct GetTMCRegisterResponse {
    uint32_t responding_to_id;
    MotorID motor_id;
    uint8_t reg;
    uint32_t data;
};

struct MotorEnableMessage {
    uint32_t id = 0;
    std::optional<bool> x = std::nullopt;
    std::optional<bool> z = std::nullopt;
    std::optional<bool> l = std::nullopt;
};

struct MoveMotorAtFrequencyMessage {
    uint32_t id;
    MotorID motor_id;
    bool direction;
    int32_t steps;
    uint32_t frequency;
};

struct MoveMotorMessage {
    uint32_t id;
    MotorID motor_id;
    bool direction;
    uint32_t frequency;
};

struct GetLimitSwitchesMessage {
    uint32_t id;
};

struct GetLimitSwitchesResponses {
    uint32_t responding_to_id;
    bool x_extend_triggered;
    bool x_retract_triggered;
    bool z_extend_triggered;
    bool z_retract_triggered;
    bool l_released_triggered;
    bool l_held_triggered;
};
struct MoveCompleteMessage {
    MotorID motor_id;
};

struct StopMotorMessage {
    uint32_t id;
};

using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, ForceUSBDisconnect,
                   ErrorMessage, AcknowledgePrevious, GetSystemInfoResponse,
                   GetTMCRegisterResponse, GetLimitSwitchesResponses>;

using SystemMessage =
    ::std::variant<std::monostate, AcknowledgePrevious, GetSystemInfoMessage,
                   SetSerialNumberMessage, EnterBootloaderMessage>;

using MotorDriverMessage =
    ::std::variant<std::monostate, SetTMCRegisterMessage, GetTMCRegisterMessage,
                   PollTMCRegisterMessage, StopPollTMCRegisterMessage>;

using MotorMessage =
    ::std::variant<std::monostate, MotorEnableMessage,
                   MoveMotorAtFrequencyMessage, MoveMotorMessage,
                   StopMotorMessage, MoveCompleteMessage,
                   GetLimitSwitchesMessage>;

};  // namespace messages
