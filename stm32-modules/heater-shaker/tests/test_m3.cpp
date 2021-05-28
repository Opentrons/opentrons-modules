#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetRPM (M3) parser works", "[gcode][parse][m3]") {
    GIVEN("a string with prefix only") {
        auto to_parse = std::array{'M', '3', ' ', 'S'};

        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a prefix matching but bad data") {
        std::string to_parse = "M3 Salsjdhas\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and float data") {
        std::string to_parse = "M3 S1000.0\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a matching prefix and a negative value") {
        std::string to_parse = "M3 S-10\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("nothing should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm == -10);
                REQUIRE(result.second == to_parse.cbegin() + 7);
            }
        }
    }

    GIVEN("a string with a matching prefix and positive integral data") {
        std::string to_parse = "M3 S1000\r\n";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm == 1000);
                REQUIRE(result.second == to_parse.cbegin() + 8);
            }
        }
    }

    GIVEN("a string with valid data and content afterwards") {
        std::string to_parse = "M3 S1000 asgasasd";
        WHEN("calling parse") {
            auto result =
                gcode::SetRPM::parse(to_parse.cbegin(), to_parse.cend());

            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.first.value().rpm == 1000);
                AND_THEN(
                    "the iterator should past just past the end of the gcode") {
                    REQUIRE(result.second == to_parse.cbegin() + 8);
                }
            }
        }
    }
}
