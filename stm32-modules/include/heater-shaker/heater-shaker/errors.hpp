#pragma once
#include <charconv>
#include <cstdint>

#include "core/utility.hpp"

namespace errors {

enum class MotorErrorOffset : uint8_t {
    FOC_DURATION = 0,
    OVER_VOLT = 1,
    UNDER_VOLT = 2,
    OVER_TEMP = 3,
    START_UP = 4,
    SPEED_FDBK = 5,
    OVERCURRENT = 6,
    SW_ERROR = 7,
};

template <typename IntType>
auto operator<<(IntType to_shift, MotorErrorOffset offset) -> IntType {
    return to_shift << static_cast<uint8_t>(offset);
}

enum class ErrorCode {
    NO_ERROR = 0,
    USB_TX_OVERRUN = 1,
    INTERNAL_QUEUE_FULL = 2,
    UNHANDLED_GCODE = 3,
    GCODE_CACHE_FULL = 4,
    BAD_MESSAGE_ACKNOWLEDGEMENT = 5,
    MOTOR_FOC_DURATION = 101,
    MOTOR_BLDC_OVERVOLT = 102,
    MOTOR_BLDC_UNDERVOLT = 103,
    MOTOR_BLDC_OVERTEMP = 104,
    MOTOR_BLDC_STARTUP_FAILED = 105,
    MOTOR_BLDC_SPEEDSENSOR_FAILED = 106,
    MOTOR_BLDC_DRIVER_FAULT = 107,
    MOTOR_BLDC_DRIVER_ERROR = 108,
    MOTOR_SPURIOUS_ERROR = 109,
    MOTOR_UNKNOWN_ERROR = 110,
    MOTOR_UNABLE_TO_MOVE = 111,
    MOTOR_ILLEGAL_SPEED = 120,
    MOTOR_ILLEGAL_RAMP_RATE = 121,
    MOTOR_BAD_HOME = 122,
    MOTOR_NOT_HOME = 123,
    MOTOR_NOT_STOPPED = 124,
    PLATE_LOCK_TIMEOUT = 125,
    PLATE_LOCK_NOT_CLOSED = 126,
    MOTOR_HOMING = 127,
    FAULTY_LATCH_SENSORS = 128,
    HEATER_THERMISTOR_A_DISCONNECTED = 201,
    HEATER_THERMISTOR_A_SHORT = 202,
    HEATER_THERMISTOR_A_OVERTEMP = 203,
    HEATER_THERMISTOR_B_DISCONNECTED = 205,
    HEATER_THERMISTOR_B_SHORT = 206,
    HEATER_THERMISTOR_B_OVERTEMP = 207,
    HEATER_THERMISTOR_BOARD_SHORT = 208,
    HEATER_THERMISTOR_BOARD_OVERTEMP = 209,
    HEATER_THERMISTOR_BOARD_DISCONNECTED = 210,
    HEATER_HARDWARE_ERROR_LATCH = 211,
    HEATER_CONSTANT_OUT_OF_RANGE = 212,
    HEATER_ILLEGAL_TARGET_TEMPERATURE = 213,
    HEATER_HARDWARE_SHORT_CIRCUIT = 214,
    HEATER_HARDWARE_OPEN_CIRCUIT = 215,
    HEATER_HARDWARE_OVERCURRENT_CIRCUIT = 216,
    SYSTEM_SERIAL_NUMBER_INVALID = 301,
    SYSTEM_SERIAL_NUMBER_HAL_ERROR = 302,
    SYSTEM_LED_I2C_NOT_READY = 303,
    SYSTEM_LED_TRANSMIT_ERROR = 304,
    SYSTEM_FLASH_ERROR = 305,
};

auto from_motor_error(uint16_t error_bitmap, MotorErrorOffset which)
    -> ErrorCode;
auto errorstring(ErrorCode code) -> const char*;

template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
constexpr auto write_into(Input start, Limit end, ErrorCode code) -> Input {
    const char* error_str = errorstring(code);
    return write_string_to_iterpair(start, end, error_str);
}

template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
constexpr auto write_into_async(Input start, Limit end, ErrorCode code)
    -> Input {
    constexpr const char* prefix = "async ";
    auto next = write_string_to_iterpair(start, end, prefix);

    const char* error_str = errorstring(code);
    return write_string_to_iterpair(next, end, error_str);
}
};  // namespace errors
