#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("OpenPlateLock (M242) parser works",
         "[gcode][parse][m242]") {
    GIVEN("a string with prefix only") {
        std::string to_parse = "M242\n";
        WHEN("calling parse") {
            auto result = gcode::OpenPlateLock::parse(
                to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M242 alsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::OpenPlateLock::parse(
                to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }
}