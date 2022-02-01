#pragma once
#include <charconv>
#include <cstdint>

#include "core/utility.hpp"

namespace errors {

enum class ErrorCode {
    // 0xx - General && comms
    NO_ERROR = 0,
    USB_TX_OVERRUN = 1,
    INTERNAL_QUEUE_FULL = 2,
    UNHANDLED_GCODE = 3,
    GCODE_CACHE_FULL = 4,
    BAD_MESSAGE_ACKNOWLEDGEMENT = 5,
    // 2XX - thermistor error
    THERMISTOR_HEATSINK_DISCONNECTED = 201,
    THERMISTOR_HEATSINK_SHORT = 202,
    THERMISTOR_HEATSINK_OVERTEMP = 203,
    THERMISTOR_FRONT_RIGHT_DISCONNECTED = 204,
    THERMISTOR_FRONT_RIGHT_SHORT = 205,
    THERMISTOR_FRONT_RIGHT_OVERTEMP = 206,
    THERMISTOR_FRONT_LEFT_DISCONNECTED = 207,
    THERMISTOR_FRONT_LEFT_SHORT = 208,
    THERMISTOR_FRONT_LEFT_OVERTEMP = 209,
    THERMISTOR_FRONT_CENTER_DISCONNECTED = 210,
    THERMISTOR_FRONT_CENTER_SHORT = 211,
    THERMISTOR_FRONT_CENTER_OVERTEMP = 212,
    THERMISTOR_BACK_RIGHT_DISCONNECTED = 213,
    THERMISTOR_BACK_RIGHT_SHORT = 214,
    THERMISTOR_BACK_RIGHT_OVERTEMP = 215,
    THERMISTOR_BACK_LEFT_DISCONNECTED = 216,
    THERMISTOR_BACK_LEFT_SHORT = 217,
    THERMISTOR_BACK_LEFT_OVERTEMP = 218,
    THERMISTOR_BACK_CENTER_DISCONNECTED = 219,
    THERMISTOR_BACK_CENTER_SHORT = 220,
    THERMISTOR_BACK_CENTER_OVERTEMP = 221,
    THERMISTOR_LID_DISCONNECTED = 222,
    THERMISTOR_LID_SHORT = 223,
    THERMISTOR_LID_OVERTEMP = 224,
    // 3xx - System General
    SYSTEM_SERIAL_NUMBER_INVALID = 301,
    SYSTEM_SERIAL_NUMBER_HAL_ERROR = 302,
    // 4xx - Thermal subsystem errors
    THERMAL_PLATE_BUSY = 401,
    THERMAL_PELTIER_ERROR = 402,
    THERMAL_HEATSINK_FAN_ERROR = 403,
    THERMAL_LID_BUSY = 404,
    THERMAL_HEATER_ERROR = 405,
    THERMAL_CONSTANT_OUT_OF_RANGE = 406,
    THERMAL_TARGET_BAD = 407,
    // 5xx - Mechanical subsystem errors
    LID_MOTOR_BUSY = 501,
    LID_MOTOR_FAULT = 502,
    SEAL_MOTOR_SPI_ERROR = 503,
    SEAL_MOTOR_BUSY = 504,
    SEAL_MOTOR_FAULT = 505,
};

auto errorstring(ErrorCode code) -> const char*;

template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
constexpr auto write_into(Input start, Limit end, ErrorCode code) -> Input {
    const char* str = errorstring(code);
    return write_string_to_iterpair(start, end, str);
}
};  // namespace errors
