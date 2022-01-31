#pragma once

#include <cstdint>

using sq0_31 = int32_t;  // 0: signed bit,  1-31: fractional bits
using q31_31 =
    uint64_t;  // 0: overflow bit, 1-32: integer bits, 33-64: fractional bits

using sq31_31 = int64_t;

/*
 * Fixed point math helpers. For now, you should only pass in two numbers with
 * the same radix position. In the future, we can expand on these helper
 * functions to account for different radix positions.
 */

auto convert_to_fixed_point(double value, int to_radix) -> sq0_31;

auto convert_to_fixed_point_64_bit(double value, int to_radix) -> sq31_31;

auto fixed_point_multiply(sq0_31 a, sq0_31 b) -> sq0_31;

auto fixed_point_multiply(sq31_31 a, sq0_31 b) -> sq0_31;
