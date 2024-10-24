#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <variant>

#include "systemwide.h"
#include "tempdeck-gen3/errors.hpp"

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
    uint32_t responding_to_id;
    errors::ErrorCode with_error = errors::ErrorCode::NO_ERROR;
};

struct IncomingMessageFromHost {
    const char* buffer;
    const char* limit;
};

// This empty message is just used to signal that the UI task should update
// its outputs
struct UpdateUIMessage {};

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

struct ThermistorReadings {
    uint32_t timestamp;
    uint32_t plate_1;
    uint32_t plate_2;
    uint32_t heatsink;
    // Peltier current feedback
    uint32_t imeas;
};

struct DeactivateAllMessage {
    uint32_t id;
};

struct GetTempDebugMessage {
    uint32_t id;
};

struct GetTempDebugResponse {
    uint32_t responding_to_id;
    float plate_temp_1;
    float plate_temp_2;
    float heatsink_temp;
    uint16_t plate_adc_1;
    uint16_t plate_adc_2;
    uint16_t heatsink_adc;
};

struct SetPeltierDebugMessage {
    uint32_t id;
    double power;
};

struct SetFanManualMessage {
    uint32_t id;
    double power;
};

struct SetFanAutomaticMessage {
    uint32_t id;
};

struct SetTemperatureMessage {
    uint32_t id;
    double target;
};

struct SetPIDConstantsMessage {
    uint32_t id;
    double p, i, d;
};

struct GetOffsetConstantsMessage {
    uint32_t id;
};

struct GetOffsetConstantsResponse {
    uint32_t responding_to_id;
    double a, b, c;
};

struct SetOffsetConstantsMessage {
    uint32_t id = 0;
    std::optional<double> a = std::nullopt;
    std::optional<double> b = std::nullopt;
    std::optional<double> c = std::nullopt;
};

struct GetThermalPowerDebugMessage {
    uint32_t id;
};

struct GetThermalPowerDebugResponse {
    uint32_t responding_to_id;
    double peltier_current, fan_rpm, peltier_pwm, fan_pwm;
};

using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, ForceUSBDisconnect,
                   ErrorMessage, AcknowledgePrevious, GetSystemInfoResponse,
                   GetTempDebugResponse, GetOffsetConstantsResponse,
                   GetThermalPowerDebugResponse>;
using SystemMessage =
    ::std::variant<std::monostate, AcknowledgePrevious, GetSystemInfoMessage,
                   SetSerialNumberMessage, EnterBootloaderMessage>;
using UIMessage = ::std::variant<std::monostate, UpdateUIMessage>;
using ThermalMessage =
    ::std::variant<std::monostate, ThermistorReadings, GetTempDebugMessage,
                   SetPeltierDebugMessage, SetFanManualMessage,
                   SetFanAutomaticMessage, DeactivateAllMessage,
                   SetTemperatureMessage, SetPIDConstantsMessage,
                   GetOffsetConstantsMessage, SetOffsetConstantsMessage,
                   GetThermalPowerDebugMessage>;
};  // namespace messages
