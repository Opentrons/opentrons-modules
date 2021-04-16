#pragma once
#include <charconv>

#include "heater-shaker/utility.hpp"

namespace errors {

enum class ErrorCode {
    NO_ERROR = 0,
    USB_TX_OVERRUN = 1,
    INTERNAL_QUEUE_FULL = 2,
    UNHANDLED_GCODE = 3,
    GCODE_CACHE_FULL = 4,
    BAD_MESSAGE_ACKNOWLEDGEMENT = 5,
};

template <typename Input, typename Limit>
requires std::forward_iterator<Input>&&
    std::sized_sentinel_for<Limit, Input> constexpr auto
    write_into(Input start, Limit end, ErrorCode code) -> Input {
    switch (code) {
        case ErrorCode::NO_ERROR:
            return start;
        case ErrorCode::USB_TX_OVERRUN: {
            constexpr const char* errstring = "ERR001:tx buffer overrun\n";
            return write_string_to_iterpair(start, end, errstring);
        }
        case ErrorCode::INTERNAL_QUEUE_FULL: {
            constexpr const char* errstring = "ERR002:internal queue full\n";
            return write_string_to_iterpair(start, end, errstring);
        }
        case ErrorCode::UNHANDLED_GCODE: {
            constexpr const char* errstring = "ERR003:unhandled gcode\n";
            return write_string_to_iterpair(start, end, errstring);
        }
        case ErrorCode::GCODE_CACHE_FULL: {
            constexpr const char* errstring = "ERR004:gcode cache full\n";
            return write_string_to_iterpair(start, end, errstring);
        }
        case ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT: {
            constexpr const char* errstring =
                "ERR005:bad message acknowledgement\n";
            return write_string_to_iterpair(start, end, errstring);
        }
        default: {
            constexpr const char* errprefix = "ERR-1:unknown error code:";
            Input written_into =
                write_string_to_iterpair(start, end, errprefix);
            auto code_res = std::to_chars(&*written_into, &*end,
                                          static_cast<uint32_t>(code));
            if (code_res.ec != std::errc()) {
                return end;
            }
            if (code_res.ptr != &*end) {
                *code_res.ptr = '\n';
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            return start + (code_res.ptr + 1 - &*start);
        }
    }
}
};  // namespace errors
