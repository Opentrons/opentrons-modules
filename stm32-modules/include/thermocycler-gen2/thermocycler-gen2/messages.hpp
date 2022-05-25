#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <variant>

#include "systemwide.h"
#include "thermocycler-gen2/colors.hpp"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/motor_utils.hpp"
#include "thermocycler-gen2/tmc2130_registers.hpp"

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
    uint32_t timestamp_ms;
};

struct LidTempReadComplete {
    uint16_t lid_temp;
    uint32_t timestamp_ms;
};

struct GetLidTemperatureDebugMessage {
    uint32_t id;
};

struct GetLidTemperatureDebugResponse {
    uint32_t responding_to_id;
    double lid_temp;
    uint16_t lid_adc;
};

struct GetLidTempMessage {
    uint32_t id;
};

struct GetLidTempResponse {
    uint32_t responding_to_id;
    double current_temp;
    double set_temp;
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

struct ActuateSolenoidMessage {
    uint32_t id;
    bool engage;
};

struct LidStepperDebugMessage {
    uint32_t id;
    double angle;
    bool overdrive;
};

struct LidStepperComplete {};

struct SealStepperDebugMessage {
    uint32_t id;
    long int steps;
};

struct SealStepperDebugResponse {
    uint32_t responding_to_id;
    long int steps_taken;
    errors::ErrorCode with_error = errors::ErrorCode::NO_ERROR;
};

struct SealStepperComplete {
    enum class CompletionReason {
        ERROR,  // There was an error flag
        STALL,  // There was a stall
        LIMIT,  // Limit switch was triggered
        DONE,   // No error
    };
    // Defaults to no-error
    CompletionReason reason = CompletionReason::DONE;
};

struct GetSealDriveStatusMessage {
    uint32_t id;
};

struct GetSealDriveStatusResponse {
    uint32_t responding_to_id;
    tmc2130::DriveStatus status;
    tmc2130::TStep tstep;
};

struct SetSealParameterMessage {
    uint32_t id;
    motor_util::SealStepper::Parameter param;
    int32_t value;
};

struct GetPlateTempMessage {
    uint32_t id;
};

struct GetPlateTempResponse {
    uint32_t responding_to_id;
    double current_temp;
    double set_temp;
    double time_remaining;
    double total_time;
    bool at_target;
};

struct SetPeltierDebugMessage {
    uint32_t id;

    double power;
    PeltierDirection direction;
    PeltierSelection selection;
};

// Can be sent to both plate task and lid task
struct GetThermalPowerMessage {
    uint32_t id;
};

// Plate Task response to GetThermalPowerMessage
struct GetPlatePowerResponse {
    uint32_t responding_to_id;

    double left, center, right, fans;
};

// Lid Task response to GetThermalPowerMessage
struct GetLidPowerResponse {
    uint32_t responding_to_id;

    double heater;
};

struct SetFanManualMessage {
    uint32_t id;
    double power;
};

struct SetHeaterDebugMessage {
    uint32_t id;
    double power;
};

struct SetLidTemperatureMessage {
    uint32_t id;
    double setpoint;
};

struct DeactivateLidHeatingMessage {
    uint32_t id;
};

struct SetPlateTemperatureMessage {
    uint32_t id;
    double setpoint;
    double hold_time;
    double volume = 0.0F;
};

struct SetFanAutomaticMessage {
    uint32_t id;
};

struct DeactivatePlateMessage {
    uint32_t id;
};

struct SetPIDConstantsMessage {
    uint32_t id;
    PidSelection selection;
    double p;
    double i;
    double d;
};

struct SetOffsetConstantsMessage {
    uint32_t id;
    bool a_set;
    double const_a;
    bool b_set;
    double const_b;
    bool c_set;
    double const_c;
};

struct GetOffsetConstantsMessage {
    uint32_t id;
};

struct GetOffsetConstantsResponse {
    uint32_t responding_to_id;
    double a, bl, cl, bc, cc, br, cr;
};

struct UpdateUIMessage {
    // Empty struct
};

struct SetLedMode {
    colors::Colors color;
    colors::Mode mode;
};

// This message is sent to the System Task by each
// subsystem task to update what the current error state is.
struct UpdateTaskErrorState {
    // Each subsystem can signal its own errors so the
    // system task can independently track whether there is
    // a reason to trigger the error light condition
    enum class Tasks : uint8_t { THERMAL_PLATE, THERMAL_LID, MOTOR };

    Tasks task;
    errors::ErrorCode current_error = errors::ErrorCode::NO_ERROR;
};

// This message is sent to the System Task by just the Thermal
// Plate Task to update what the current state of the thermal
// subsystem is. This dictates how the UI LED's are controlled
// if there is no active error flag.
struct UpdatePlateState {
    enum class PlateState : uint8_t {
        IDLE,
        HEATING,
        AT_HOT_TEMP,
        COOLING,
        AT_COLD_TEMP
    };

    PlateState state;
};

struct GetLidStatusMessage {
    uint32_t id;
};

struct GetLidStatusResponse {
    uint32_t responding_to_id;
    motor_util::LidStepper::Position lid;
    motor_util::SealStepper::Status seal;
};

struct OpenLidMessage {
    uint32_t id;
};

struct CloseLidMessage {
    uint32_t id;
};

struct PlateLiftMessage {
    uint32_t id;
};

struct FrontButtonPressMessage {};

// This is a two-stage message that is first sent to the Plate task,
// and then the Lid task.
struct DeactivateAllMessage {
    uint32_t id;
};

struct DeactivateAllResponse {
    uint32_t responding_to_id;
};

struct GetLidSwitchesMessage {
    uint32_t id;
};

struct GetLidSwitchesResponse {
    uint32_t responding_to_id;
    bool close_switch_pressed;
    bool open_switch_pressed;
    bool seal_switch_pressed;
};

struct GetFrontButtonMessage {
    uint32_t id;
};

struct GetFrontButtonResponse {
    uint32_t responding_to_id;
    bool button_pressed;
};

using SystemMessage =
    ::std::variant<std::monostate, EnterBootloaderMessage, AcknowledgePrevious,
                   SetSerialNumberMessage, GetSystemInfoMessage,
                   UpdateUIMessage, SetLedMode, UpdateTaskErrorState,
                   UpdatePlateState, GetFrontButtonMessage>;
using HostCommsMessage = ::std::variant<
    std::monostate, IncomingMessageFromHost, AcknowledgePrevious, ErrorMessage,
    ForceUSBDisconnectMessage, GetSystemInfoResponse,
    GetLidTemperatureDebugResponse, GetPlateTemperatureDebugResponse,
    GetPlateTempResponse, GetLidTempResponse, GetSealDriveStatusResponse,
    GetLidStatusResponse, GetPlatePowerResponse, GetLidPowerResponse,
    GetOffsetConstantsResponse, SealStepperDebugResponse, DeactivateAllResponse,
    GetLidSwitchesResponse, GetFrontButtonResponse>;
using ThermalPlateMessage =
    ::std::variant<std::monostate, ThermalPlateTempReadComplete,
                   GetPlateTemperatureDebugMessage, SetPeltierDebugMessage,
                   SetFanManualMessage, GetPlateTempMessage,
                   SetPlateTemperatureMessage, DeactivatePlateMessage,
                   SetPIDConstantsMessage, SetFanAutomaticMessage,
                   GetThermalPowerMessage, SetOffsetConstantsMessage,
                   GetOffsetConstantsMessage, DeactivateAllMessage>;
using LidHeaterMessage =
    ::std::variant<std::monostate, LidTempReadComplete,
                   GetLidTemperatureDebugMessage, SetHeaterDebugMessage,
                   GetLidTempMessage, SetLidTemperatureMessage,
                   DeactivateLidHeatingMessage, SetPIDConstantsMessage,
                   GetThermalPowerMessage, DeactivateAllMessage>;
using MotorMessage = ::std::variant<
    std::monostate, ActuateSolenoidMessage, LidStepperDebugMessage,
    LidStepperComplete, SealStepperDebugMessage, SealStepperComplete,
    GetSealDriveStatusMessage, SetSealParameterMessage, GetLidStatusMessage,
    OpenLidMessage, CloseLidMessage, PlateLiftMessage, FrontButtonPressMessage,
    GetLidSwitchesMessage>;
};  // namespace messages
