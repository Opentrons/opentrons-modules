#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("ClosePlateLock (M243) parser works",
         "[gcode][parse][m243]") {
    GIVEN("a string with prefix only") {
        std::string to_parse = "M243\n";
        WHEN("calling parse") {
            auto result = gcode::ClosePlateLock::parse(
                to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M243 alsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::ClosePlateLock::parse(
                to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }
}