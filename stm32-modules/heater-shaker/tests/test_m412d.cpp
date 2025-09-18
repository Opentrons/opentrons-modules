#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetErrorStateDebug parser works", "[gcode][parse][m412.d]") {
    GIVEN("an empty string") {
        std::string to_parse = "";
        WHEN("calling parse") {
            auto result = gcode::SetErrorStateDebug::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE_FALSE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a fully non-matching string") {
        std::string to_parse = "asdhalghasdasd ";

        WHEN("calling parse") {
            auto result = gcode::SetErrorStateDebug::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE_FALSE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "M4asdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetErrorStateDebug::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE_FALSE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode but no E argument") {
        std::string to_parse = "M412.D T21312\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetErrorStateDebug::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE_FALSE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode and E argument but no T") {
        std::string to_parse = "M412.D E209\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetErrorStateDebug::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first->error ==
                        errors::ErrorCode::HEATER_THERMISTOR_BOARD_OVERTEMP);
                REQUIRE(result.first->delay_s == 0);
            }
        }
    }

    GIVEN("a string with a good gcode and both arguments") {
        std::string to_parse = "M412.D E209 T1250\r\n";
        WHEN("calling parse") {
            auto result = gcode::SetErrorStateDebug::parse(to_parse.cbegin(),
                                                           to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first->error ==
                        errors::ErrorCode::HEATER_THERMISTOR_BOARD_OVERTEMP);
                REQUIRE(result.first->delay_s == 1250);
            }
        }
    }
}
