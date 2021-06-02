#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop


SCENARIO("ActuateSolenoid (G28.D) parser works", "[gcode][parse][g28.d]") {
    GIVEN("a string with prefix only") {
        std::string to_parse = "G28.D S\n";

        WHEN("calling parse") {
            auto result = gcode::ActuateSolenoid::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "G28.D Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result = gcode::ActuateSolenoid::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with good data") {
        std::string to_parse = "G28.D S223\r\n";
        WHEN("calling parse") {
            auto result = gcode::ActuateSolenoid::parse(to_parse.cbegin(),
                                                        to_parse.cend());

            THEN("the data should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().current_ma == 223);
                REQUIRE(result.second == to_parse.cbegin() + 10);
            }
        }
    }
}
