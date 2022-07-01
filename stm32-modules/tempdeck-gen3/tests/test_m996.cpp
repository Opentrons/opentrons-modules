#include <array>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "tempdeck-gen3/errors.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "tempdeck-gen3/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetSerialNumber (M996) parser works", "[gcode][parse][m996]") {
    GIVEN("a string with prefix only") {
        auto to_parse = std::array{'M', '9', '9', '6', ' '};

        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M996 -100000xxxxxxxxxxxxxxxx\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("serial should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().value ==
                        std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                            "-100000xxxxxxxxxxxxxxxx"});
                REQUIRE(result.second == to_parse.cbegin() + 28);
            }
        }
    }

    GIVEN("a string with a matching prefix and positive integral data") {
        std::string to_parse = "M996 1000000xxxxxxxxxxxxxxxx\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().value ==
                        std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                            "1000000xxxxxxxxxxxxxxxx"});
                REQUIRE(result.second == to_parse.cbegin() + 28);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M996 1000000xxxxxxxxxxxxxxxx asgasasd\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().value ==
                        std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                            "1000000xxxxxxxxxxxxxxxx"});
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 28);
                }
            }
        }
    }

    GIVEN("a string with too much valid data") {
        std::string to_parse = "M996 1000000Axxxxxxxxxxxxxxxxx\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN(
                "nothing should be parsed and error message should be passed "
                "back") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with less than default valid data") {
        std::string to_parse = "M996 10000\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().value ==
                        std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                            "10000"});
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 10);
                }
            }
        }
    }
}
