#include <array>
#include <span>

#include "catch2/catch.hpp"
#include "ot_utils/core/bit_utils.hpp"

using namespace ot_utils;

SCENARIO("bytes_to_int works") {
    GIVEN("a 2 byte input") {
        auto arr = std::array<uint8_t, 2>{1, 2};
        WHEN("parsing 2 bytes from both bytes") {
            uint16_t val = 0;
            auto next = bit_utils::bytes_to_int(arr.cbegin(), arr.cend(), val);
            THEN("it is converted to a uint16_t") { REQUIRE(val == 0x0102); }
            THEN("the return iterator points to the end") {
                REQUIRE(next == arr.cend());
            }
        }
        WHEN("parsing 2 bytes from only 1 byte") {
            uint16_t val = 0;
            auto next =
                bit_utils::bytes_to_int(arr.cbegin(), arr.cbegin() + 1, val);
            THEN("only the first byte is read") { REQUIRE(val == 0x0100); }
            THEN("the return points to the second byte") {
                REQUIRE(next == (arr.cbegin() + 1));
            }
        }
        WHEN("parsing 1 byte from 2 bytes") {
            uint8_t val = 0;
            auto next = bit_utils::bytes_to_int(arr.cbegin(), arr.cend(), val);
            THEN("only the first byte is read") { REQUIRE(val == 0x01); }
            THEN("the return points to the second byte") {
                REQUIRE(next == (std::next(arr.cbegin())));
            }
        }
        WHEN("parsing from a view") {
            uint16_t val = 0;
            auto next = bit_utils::bytes_to_int(std::views::all(arr), val);
            THEN("The parse should provide the right number") {
                REQUIRE(val == 0x0102);
                REQUIRE(next == arr.cend());
            }
        }
    }
    GIVEN("a 4 byte input") {
        auto arr = std::array<uint8_t, 4>{0xFF, 0xef, 0x3, 0x1};

        WHEN("parsing 4 bytes") {
            uint32_t val = 0;
            auto next = bit_utils::bytes_to_int(arr.cbegin(), arr.cend(), val);
            THEN("it is converted to a uint32_t") {
                REQUIRE(val == 0xFFEF0301);
            }
            THEN("the return points to the end") {
                REQUIRE(next == arr.cend());
            }
        }

        WHEN("called with a 2 byte output request") {
            uint16_t val = 0;
            auto next = bit_utils::bytes_to_int(arr, val);
            THEN("only the first two bytes are converted") {
                REQUIRE(val == 0xFFEF);
            }
            THEN("the return points to 2 bytes in") {
                REQUIRE(next == (arr.cbegin() + 2));
            }
        }

        WHEN("called with an 8 byte output request") {
            uint64_t val = 0;
            auto next = bit_utils::bytes_to_int(arr, val);
            THEN("only the first 4 bytes of the output are filled") {
                REQUIRE(val == 0xffef030100000000);
            }
            THEN("the iterator points to the end") {
                REQUIRE(next == arr.cend());
            }
        }
    }
    GIVEN("a 1 byte span") {
        auto arr = std::array<uint8_t, 1>{0xdd};
        WHEN("called with a one byte request") {
            uint8_t val = 0;
            auto next = bit_utils::bytes_to_int(arr.cbegin(), arr.cend(), val);
            THEN("it is converted to a uint8_t") { REQUIRE(val == 0xDD); }
            THEN("the iterator points to the end") {
                REQUIRE(next == arr.cend());
            }
        }
    }
}

SCENARIO("int_to_bytes works") {
    GIVEN("some integers") {
        auto arr = std::array<uint8_t, 7>{};
        uint32_t i32 = 0x01234567;
        uint16_t i16 = 0x89ab;
        uint8_t i8 = 0xcd;
        auto iter = arr.begin();

        WHEN("called") {
            iter = bit_utils::int_to_bytes(i32, iter, arr.end());
            iter = bit_utils::int_to_bytes(i16, iter, arr.end());
            iter = bit_utils::int_to_bytes(i8, iter, arr.end());
            THEN("The values are written into the array") {
                REQUIRE(arr[0] == 0x01);
                REQUIRE(arr[1] == 0x23);
                REQUIRE(arr[2] == 0x45);
                REQUIRE(arr[3] == 0x67);
                REQUIRE(arr[4] == 0x89);
                REQUIRE(arr[5] == 0xab);
                REQUIRE(arr[6] == 0xcd);
            }
            AND_WHEN("converting data back") {
                iter = arr.begin();
                uint32_t i32_out;
                uint16_t i16_out;
                uint8_t i8_out;
                iter = bit_utils::bytes_to_int(iter, arr.end(), i32_out);
                iter = bit_utils::bytes_to_int(iter, arr.end(), i16_out);
                iter = bit_utils::bytes_to_int(iter, arr.end(), i8_out);
                THEN("values are identical") {
                    REQUIRE(i32 == i32_out);
                    REQUIRE(i16 == i16_out);
                    REQUIRE(i8 == i8_out);
                }
            }
        }
    }
}
