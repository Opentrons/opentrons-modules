#include "ot_utils/core/fixed_point.hpp"

/*
 * Fixed point math helpers. For now, you should only pass in two numbers with
 * the same radix position. In the future, we can expand on these helper
 * functions to account for different radix positions.
 */

static constexpr int64_t multiply_mask = 0xFFFFFFFF;
static constexpr int multiply_shift = 31;

auto ot_utils::fixed_point::convert_to_fixed_point(double value, int to_radix)
    -> sq0_31 {
    return static_cast<int32_t>(value * static_cast<double>(1LL << to_radix));
}

auto ot_utils::fixed_point::convert_to_fixed_point_64_bit(double value,
                                                          int to_radix)
    -> sq31_31 {
    return static_cast<int64_t>(value * static_cast<double>(1LL << to_radix));
}

auto ot_utils::fixed_point::fixed_point_multiply(sq0_31 a, sq0_31 b) -> sq0_31 {
    int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(b);
    return static_cast<sq0_31>((result >> multiply_shift) & multiply_mask);
}

auto ot_utils::fixed_point::fixed_point_multiply(sq31_31 a, sq0_31 b)
    -> sq0_31 {
    int64_t result = a * static_cast<int64_t>(b);
    return static_cast<sq0_31>((result >> multiply_shift) & multiply_mask);
}
