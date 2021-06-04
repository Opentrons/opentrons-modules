#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("DebugControlPlateLockMotor (M240.D) parser works", "[gcode][parse][m104.d]") {
    GIVEN("a string with prefix only") {
        std::string to_parse = "M240.D S\n";
        WHEN("calling parse") {
            auto result = gcode::DebugControlPlateLockMotor::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M240.D Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::DebugControlPlateLockMotor::parse(to_parse.cbegin(),
                                                                   to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with good data") {
        std::string to_parse = "M240.D S-0.5\r\n";
        WHEN("calling parse") {
            auto result = gcode::DebugControlPlateLockMotor::parse(to_parse.cbegin(),
                                                                   to_parse.cend());

            THEN("the data should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().power == -0.5);
                REQUIRE(result.second == to_parse.cend() - 2);
            }
        }
    }
}
