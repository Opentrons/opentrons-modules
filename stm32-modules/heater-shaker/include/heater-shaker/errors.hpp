#pragma once
#include <charconv>
#include <cstdint>

#include "heater-shaker/utility.hpp"

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
    MOTOR_REQUESTED_SPEED_INVALID = 100,
    MOTOR_FOC_DURATION = 101,
    MOTOR_BLDC_OVERVOLT = 102,
    MOTOR_BLDC_UNDERVOLT = 103,
    MOTOR_BLDC_OVERTEMP = 104,
    MOTOR_BLDC_STARTUP_FAILED = 105,
    MOTOR_BLDC_SPEEDSENSOR_FAILED = 106,
    MOTOR_BLDC_OVERCURRENT = 107,
    MOTOR_BLDC_DRIVER_ERROR = 108,
    MOTOR_SPURIOUS_ERROR = 109,
    MOTOR_UNKNOWN_ERROR = 110,
    MOTOR_ILLEGAL_SPEED = 120,
    MOTOR_ILLEGAL_RAMP_RATE = 121,
};

auto from_motor_error(uint16_t error_bitmap, MotorErrorOffset which)
    -> ErrorCode;
auto errorstring(ErrorCode code) -> const char*;

template <typename Input, typename Limit>
requires std::forward_iterator<Input>&&
    std::sized_sentinel_for<Limit, Input> constexpr auto
    write_into(Input start, Limit end, ErrorCode code) -> Input {
    const char* str = errorstring(code);
    return write_string_to_iterpair(start, end, str);
}
};  // namespace errors
