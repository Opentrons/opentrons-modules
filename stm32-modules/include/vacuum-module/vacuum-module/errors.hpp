#pragma once
#include <charconv>
#include <cstdint>

#include "core/utility.hpp"

namespace errors {

enum class ErrorCode : uint16_t {
    // 0xx - General && comms
    NO_ERROR = 0,
    USB_TX_OVERRUN = 1,
    INTERNAL_QUEUE_FULL = 2,
    UNHANDLED_GCODE = 3,
    GCODE_CACHE_FULL = 4,
    BAD_MESSAGE_ACKNOWLEDGEMENT = 5,
    TASK_NOT_READY = 7,
    DEBUG_MESSAGE = 8,
    // 3xx - System General
    SYSTEM_SERIAL_NUMBER_INVALID = 301,
    SYSTEM_SERIAL_NUMBER_HAL_ERROR = 302,
    SYSTEM_EEPROM_ERROR = 303,
    // 4xx - Vacuum Errors
    PRESSURE_NOT_REACHED_ERROR = 400,
    WASTE_FULL_ERROR = 401,
    VENT_FAILED_ERROR = 402,
    // 5xx - Pump Errors
};

auto errorstring(ErrorCode code) -> const char*;

template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
constexpr auto write_into(Input start, Limit end, ErrorCode code) -> Input {
    const char* str = errorstring(code);
    auto next = write_string_to_iterpair(start, end, str);

    constexpr const char* suffix = " OK\n";
    return write_string_to_iterpair(next, end, suffix);
}

template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
constexpr auto write_into_async(Input start, Limit end, ErrorCode code, const char* message = nullptr)
    -> Input {
    constexpr const char* prefix = "async ";
    auto next = write_string_to_iterpair(start, end, prefix);

    const char* error_str = errorstring(code);
    next = write_string_to_iterpair(next, end, error_str);

    // Optional message
    if (message != nullptr) {
        next = write_string_to_iterpair(next, end, message);
    }

    constexpr const char* suffix = "\n";
    return write_string_to_iterpair(next, end, suffix);
}
};  // namespace errors
