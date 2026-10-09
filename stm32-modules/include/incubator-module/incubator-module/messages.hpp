#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <variant>

#include "incubator-module/errors.hpp"
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

struct DebugMessage {
    static constexpr std::size_t MAX_LENGTH = DEBUG_MESSAGE_LENGTH;
    std::optional<std::array<char, MAX_LENGTH>> message = std::nullopt;
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

struct GetResetReasonMessage {
    uint32_t id;
};

struct GetResetReasonResponse {
    uint32_t responding_to_id;
    uint16_t reason;
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

struct GetCapacitiveStateMessage {
    uint32_t id = 0;
};

struct GetCapacitiveStateResponseMessage {
    uint32_t responding_to_id;
    bool with_error = false;
    double capacitive_ch1;
    double capacitive_ch2;
    double capacitive_ch3;
    double capacitive_ch4;
};

struct GetProximityStateMessage {
    uint32_t id = 0;
};

struct GetProximityStateResponseMessage {
    uint32_t responding_to_id;
    bool pc2_detected;
};

struct ForceUSBDisconnect {
    uint32_t id;
    size_t return_address;
};

struct SetMotorCurrentMessage {
    uint32_t id;
    MotorID motor_id;
    float run_current;
    float hold_current;
};

struct SetMicrostepsMessage {
    uint32_t id;
    MotorID motor_id;
    uint8_t microsteps_power;
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
    std::optional<bool> r = std::nullopt;
};

struct MoveMotorInStepsMessage {
    uint32_t id;
    MotorID motor_id;
    int32_t steps;
    uint32_t steps_per_second;
    uint32_t steps_per_second_sq;
};

struct MoveMotorInMmMessage {
    uint32_t id = 0;
    MotorID motor_id = MotorID::MOTOR_R;
    float mm = 0;
    std::optional<float> mm_per_second = std::nullopt;
    std::optional<float> mm_per_second_sq = std::nullopt;
    std::optional<float> mm_per_second_discont = std::nullopt;
};

struct MoveCompleteMessage {
    MotorID motor_id;
};

struct StopMotorMessage {
    uint32_t id;
    MotorID motor_id;
};

struct GetMoveParamsMessage {
    uint32_t id;
    MotorID motor_id;
};

struct GetMoveParamsResponse {
    uint32_t responding_to_id;
    MotorID motor_id;
    float velocity;
    float acceleration;
    float velocity_discont;
};

struct SetDiag0IRQMessage {
    bool enable;
};

struct SetMotorStallGuardMessage {
    uint32_t id = 0;
    MotorID motor_id = MotorID::MOTOR_R;
    bool enable = false;
    std::optional<int32_t> sgt = std::nullopt;
};

struct GetMotorStallGuardMessage {
    uint32_t id = 0;
    MotorID motor_id = MotorID::MOTOR_R;
};

struct GetMotorStallGuardResponse {
    uint32_t responding_to_id;
    MotorID motor_id;
    bool enabled;
    int sgt;
};

struct HomeMotorMessage {
    uint32_t id;
    MotorID motor_id;
    bool direction;
};

using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, ForceUSBDisconnect,
                   ErrorMessage, DebugMessage, AcknowledgePrevious,
                   GetSystemInfoResponse, GetResetReasonResponse,
                   GetCapacitiveStateResponseMessage,
                   GetProximityStateResponseMessage, GetTMCRegisterResponse,
                   GetMoveParamsResponse, GetMotorStallGuardResponse>;

using MotorDriverMessage =
    ::std::variant<std::monostate, SetTMCRegisterMessage, GetTMCRegisterMessage,
                   PollTMCRegisterMessage, StopPollTMCRegisterMessage,
                   SetMotorCurrentMessage, SetMicrostepsMessage,
                   SetMotorStallGuardMessage, GetMotorStallGuardMessage>;

using MotorMessage =
    ::std::variant<std::monostate, MotorEnableMessage, MoveMotorInStepsMessage,
                   StopMotorMessage, MoveCompleteMessage, MoveMotorInMmMessage,
                   SetMicrostepsMessage, GetMoveParamsMessage,
                   SetDiag0IRQMessage, HomeMotorMessage>;

using SystemMessage =
    ::std::variant<std::monostate, AcknowledgePrevious, GetSystemInfoMessage,
                   SetSerialNumberMessage, EnterBootloaderMessage,
                   GetResetReasonMessage>;

using UIMessage = ::std::variant<std::monostate>;

using CapacitiveMessage =
    ::std::variant<std::monostate, GetCapacitiveStateMessage>;

using ProximityMessage =
    ::std::variant<std::monostate, GetProximityStateMessage>;

};  // namespace messages
