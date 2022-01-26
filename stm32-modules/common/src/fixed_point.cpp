#include "core/fixed_point.hpp"

/*
 * Fixed point math helpers. For now, you should only pass in two numbers with
 * the same radix position. In the future, we can expand on these helper
 * functions to account for different radix positions.
 */

auto convert_to_fixed_point(float value, int to_radix) -> sq0_31 {
    return static_cast<int32_t>(value * static_cast<float>(1LL << to_radix));
}

auto convert_to_fixed_point_64_bit(float value, int to_radix) -> sq31_31 {
    return static_cast<int64_t>(value * static_cast<float>(1LL << to_radix));
}

auto fixed_point_multiply(sq0_31 a, sq0_31 b) -> sq0_31 {
    int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(b);
    return static_cast<sq0_31>((result >> 31) & 0xFFFFFFFF);
}

auto fixed_point_multiply(sq31_31 a, sq0_31 b) -> sq0_31 {
    int64_t result = a * static_cast<int64_t>(b);
    return static_cast<sq0_31>((result >> 31) & 0xFFFFFFFF);
}
