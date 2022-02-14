#include "catch2/catch.hpp"
#include "core/fixed_point.hpp"

auto convert_to_integer(float f, int conversion) {
    return static_cast<int32_t>(f * static_cast<float>(1LL << conversion));
}

auto convert_to_integer_64(float f, int conversion) {
    return static_cast<int64_t>(f * static_cast<float>(1LL << conversion));
}

SCENARIO("Fixed point multiplication") {
    GIVEN("Two integers") {
        WHEN("both are positive") {
            int32_t a = convert_to_integer(0.3, 31);
            int32_t b = convert_to_integer(0.5, 31);
            auto result = fixed_point_multiply(a, b);
            THEN("the result should be 0.15") {
                int32_t expected = convert_to_integer(0.15, 31);
                REQUIRE(result == expected);
            }
        }
        WHEN("one is positive and one is negative") {
            int32_t a = convert_to_integer(0.5, 31);
            int32_t b = convert_to_integer(-0.75, 31);

            auto result = fixed_point_multiply(a, b);
            THEN("the result should be -0.375") {
                int32_t expected = convert_to_integer(-0.375, 31);
                REQUIRE(result == expected);
            }
        }
        WHEN("both are negative") {
            int32_t a = convert_to_integer(-0.25, 31);
            int32_t b = convert_to_integer(-0.25, 31);

            auto result = fixed_point_multiply(a, b);
            THEN("the result should be 0.4") {
                int32_t expected = convert_to_integer(0.0625, 31);
                REQUIRE(result == expected);
            }
        }

        WHEN("the sizes are different") {
            int64_t a = convert_to_integer_64(2, 31);
            int32_t b = convert_to_integer(0.25, 31);

            auto result = fixed_point_multiply(a, b);
            THEN("the result should be 0.4") {
                int32_t expected = convert_to_integer(0.5, 31);
                REQUIRE(result == expected);
            }
        }
    }
}
