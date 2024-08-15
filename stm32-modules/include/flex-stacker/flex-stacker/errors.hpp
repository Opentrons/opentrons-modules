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
    // 3xx - System General
    SYSTEM_SERIAL_NUMBER_INVALID = 301,
    SYSTEM_SERIAL_NUMBER_HAL_ERROR = 302,
    SYSTEM_EEPROM_ERROR = 303,
    // 9xx - TMC2160
    TMC2160_READ_ERROR = 901,
    TMC2160_WRITE_ERROR = 902,
    TMC2160_INVALID_ADDRESS = 903,
    // 4xx - Motor Errors
    MOTOR_ENABLE_FAILED = 401,
    MOTOR_DISABLE_FAILED = 402,
};

auto errorstring(ErrorCode code) -> const char*;

template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
constexpr auto write_into(Input start, Limit end, ErrorCode code) -> Input {
    const char* str = errorstring(code);
    return write_string_to_iterpair(start, end, str);
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
