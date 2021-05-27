#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetHeaterPowerTest (M104.D) parser works", "[gcode][parse][m104.d]") {
    GIVEN("a string with prefix only") {
        std::string to_parse = "M104.D S\n";

        WHEN("calling parse") {
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M104.D Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
                                                           to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with good data") {
        std::string to_parse = "M104.D S0.5\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetHeaterPowerTest::parse(to_parse.cbegin(),
                                                           to_parse.cend());

            THEN("the data should be parsed") {
                REQUIRE(result.first.has_value());
            }
        }
    }
}
