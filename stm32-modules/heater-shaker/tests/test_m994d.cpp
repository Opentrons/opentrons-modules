#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#include "systemwide.h"
#pragma GCC diagnostic pop

SCENARIO("SetLEDDebug (M994.D) parser works", "[gcode][parse][m994.d]") {
    GIVEN("a string with prefix only") {
        std::string to_parse = "M994.D \n";
        WHEN("calling parse") {
            auto result =
                gcode::SetLEDDebug::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M994.D Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetLEDDebug::parse(to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with good data") {
        std::string to_parse = "M994.D 1\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetLEDDebug::parse(to_parse.cbegin(), to_parse.cend());

            THEN("the data should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().color == RED);
                REQUIRE(result.second == to_parse.cend() - 2);
            }
        }
    }
}
