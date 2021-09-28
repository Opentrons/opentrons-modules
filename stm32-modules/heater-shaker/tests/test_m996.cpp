#include <array>
#include "systemwide.hpp"

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
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

    /*GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M996 alsjdhas\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetSerialNumber::parse(to_parse.cbegin(),
    to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }*/

    /*GIVEN("a string with a matching prefix and float data") {
        std::string to_parse = "M996 10000.0\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetSerialNumber::parse(to_parse.cbegin(),
    to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }*/

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M996 -100000xxxxxxxxxxxxxxxx\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().serial_number ==
                        std::array<char, systemwide::serial_number_length>{"-100000xxxxxxxxxxxxxxxx"});
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
                REQUIRE(result.first.value().serial_number ==
                        std::array<char, systemwide::serial_number_length>{"1000000xxxxxxxxxxxxxxxx"});
                REQUIRE(result.second == to_parse.cbegin() + 28);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M996 1000000xxxxxxxxxxxxxxxx asgasasd";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().serial_number ==
                        std::array<char, systemwide::serial_number_length>{"1000000xxxxxxxxxxxxxxxx"});
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 28);
                }
            }
        }
    }

    /*GIVEN("a string with too much valid data") {
        std::string to_parse = "M996 1000000A";
        WHEN("calling parse") {
            auto result =
                gcode::SetSerialNumber::parse(to_parse.cbegin(),
    to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().serial_number == std::array<char,8>
    {"1000000"}); AND_THEN( "the iterator should past just past the end of the
    gcode") { REQUIRE(result.second == to_parse.cbegin() + 12);
                }
            }
        }
    }*/

    GIVEN("a string with less than default valid data") {
        std::string to_parse = "M996 10000";
        WHEN("calling parse") {
            auto result = gcode::SetSerialNumber::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().serial_number ==
                        std::array<char, systemwide::serial_number_length>{"10000"});
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 10);
                }
            }
        }
    }
}
