#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <variant>

#include "systemwide.h"
#include "vacuum-module/errors.hpp"

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

struct ForceUSBDisconnect {
    uint32_t id;
    size_t return_address;
};

struct SetStatusBarStateMessage {
    uint32_t id = 0;
    bool from_host = false;
    std::optional<StatusBarID> bar_id = std::nullopt;
    std::optional<StatusBarColor> color = std::nullopt;
    std::optional<StatusBarPattern> pattern = std::nullopt;
    std::optional<uint32_t> duration = std::nullopt;
    std::optional<int8_t> reps = std::nullopt;
    std::optional<float> power = std::nullopt;
};

// For internal driving
struct PressureControlMessage {};
struct PumpControlMessage {};

struct SetPressureStateMessage {
    uint32_t id = 0;
    double pressure_setpoint = 0;
    uint32_t duration_s = 0;
    double ramp_rate = 0;
    bool start_pump = false;
    bool vent_after = false;
};

struct SetPumpStateMessage {
    uint32_t id = 0;
    bool from_host = false;
    double rpm_setpoint = 0;
    uint8_t duty_cycle = 0;
    bool run_pump = false;
};

struct GetPressureStateMessage {
    uint32_t id = 0;
};

struct GetPressureStateResponseMessage {
    uint32_t responding_to_id;
    double target_pressure;
    double current_pressure;
    double pressure_abs_a;
    double pressure_abs_b;
    double pressure_atm;
    bool vacuum_enabled;
    VentState vent_state;
};

struct GetPumpStateMessage {
    uint32_t id = 0;
};

struct GetPumpStateResponseMessage {
    uint32_t responding_to_id;
    double target_rpm;
    double current_rpm;
    uint8_t target_pwm;
    uint8_t current_pwm;
    bool pump_running;
    bool manual_control;
};

struct SetVentMessage {
    uint32_t id = 0;
    bool from_host = false;
    VentState state = VentState::CLOSED;
};

struct SetPressurePIDMessage {
    uint32_t id = 0;
    std::optional<double> kp = std::nullopt;
    std::optional<double> ki = std::nullopt;
    std::optional<double> kd = std::nullopt;
    std::optional<double> overshoot = std::nullopt;
    std::optional<double> k_velocity = std::nullopt;
    std::optional<double> k_holding = std::nullopt;
    bool reset = false;
};

struct GetPressurePIDMessage {
    uint32_t id = 0;
};

struct GetPressurePIDResponseMessage {
    uint32_t responding_to_id;
    double kp;
    double ki;
    double kd;
    double overshoot;
    double k_velocity;
    double k_holding;
};

using HostCommsMessage =
    ::std::variant<std::monostate, IncomingMessageFromHost, ForceUSBDisconnect,
                   ErrorMessage, DebugMessage, AcknowledgePrevious,
                   GetSystemInfoResponse, GetResetReasonResponse,
                   GetPressureStateResponseMessage, GetPumpStateResponseMessage,
                   GetPressurePIDResponseMessage>;

using SystemMessage =
    ::std::variant<std::monostate, AcknowledgePrevious, GetSystemInfoMessage,
                   SetSerialNumberMessage, EnterBootloaderMessage,
                   GetResetReasonMessage>;

using UIMessage = ::std::variant<std::monostate, SetStatusBarStateMessage>;

using PressureMessage =
    ::std::variant<std::monostate, PressureControlMessage,
                   SetPressureStateMessage, GetPressureStateMessage,
                   SetVentMessage, SetPressurePIDMessage,
                   GetPressurePIDMessage>;

using PumpMessage = ::std::variant<std::monostate, PumpControlMessage,
                                   SetPumpStateMessage, GetPumpStateMessage>;

};  // namespace messages
