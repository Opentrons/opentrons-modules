#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop


SCENARIO("SetAcceleration (M124) parser works", "[gcode][parse][m124]") {
    GIVEN("a string with prefix only") {
        auto to_parse = std::array{'M', '1', '2', '4', ' ', 'S'};

        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M204 Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and float data") {
        std::string to_parse = "M204 S1000.0\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M204 S-10\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm_per_s == -10);
                REQUIRE(result.second == to_parse.cbegin() + 9);
            }
        }
    }

    GIVEN("a string with a matching prefix and positive integral data") {
        std::string to_parse = "M204 S1000\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm_per_s == 1000);
                REQUIRE(result.second == to_parse.cbegin() + 10);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M204 S1000 asgasasd";
        WHEN("calling parse") {
            auto result = gcode::SetAcceleration::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm_per_s == 1000);
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 10);
                }
            }
        }
    }
}
