#pragma once
#include <algorithm>
#include <concepts>
#include <cstring>

namespace errors {
template <typename ErrorType>
concept Error = requires(ErrorType err) {
    { err.errorstring() }
    ->std::same_as<const char*>;
};

template <typename ErrorType>
requires Error<ErrorType> constexpr auto write_into(char* buf, size_t available)
    -> size_t {
    auto* wrote_to = std::copy(
        ErrorType::errorstring(),
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ErrorType::errorstring() +
            std::min(strlen(ErrorType::errorstring()), available),
        buf);
    return &(*wrote_to) - buf;
}
struct USBTXBufOverrun {
    static constexpr auto errorstring() -> const char* {
        return "ERR001:tx buffer overrun\n";
    }
    static constexpr auto code() -> uint32_t { return 1; }
};
struct InternalQueueFull {
    static constexpr auto errorstring() -> const char* {
        return "ERR002:internal queue full\n";
    }
    static constexpr auto code() -> uint32_t { return 2; }
};
struct UnhandledGCode {
    static constexpr auto errorstring() -> const char* {
        return "ERR003:unhandled or unknown gcode\n";
    }
    static constexpr auto code() -> uint32_t { return 3; }
};
struct GCodeCacheFull {
    static constexpr auto errorstring() -> const char* {
        return "ERR004:gcode cache full\n";
    }
    static constexpr auto code() -> uint32_t { return 4; }
};

struct BadMessageAcknowledgement {
    static constexpr auto errorstring() -> const char* {
        return "ERR005:bad message ack\n";
    }
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    static constexpr auto code() -> uint32_t { return 5; }
};
};  // namespace errors
