#pragma once

#include <algorithm>
#include <cstring>
#include <iterator>

template <typename Input, typename InLimit, typename Output, typename OutLimit>
requires std::forward_iterator<Input> && std::forward_iterator<Output> &&
    std::sized_sentinel_for<Input, InLimit> &&
    std::sized_sentinel_for<Output, OutLimit>
constexpr auto copy_min_range(Input dest_start, InLimit dest_limit,
                              Output source_start, OutLimit source_end)
    -> Input {
    return std::copy(
        source_start,
        std::min(source_end, source_start + (dest_limit - dest_start)),
        dest_start);
}

template <typename Input, typename InLimit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Input, InLimit>
constexpr auto write_string_to_iterpair(Input start, InLimit end,
                                        const char* str) -> Input {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return copy_min_range(start, end, str, str + strlen(str));
}
