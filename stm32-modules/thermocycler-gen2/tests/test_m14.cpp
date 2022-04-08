#include "catch2/catch.hpp"

// Push this diagnostic to avoid a compiler error about printing to too
// small of a buffer... which we're doing on purpose!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-gen2/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("DeactivatePlate (M14) parser works", "[gcode][parse][m14]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::DeactivatePlate::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M14 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::DeactivatePlate::write_response_into(
                buffer.begin(), buffer.begin() + 5);
            THEN("the response should write only up to the available space") {
                std::string response = "M14 Occccccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a valid input") {
        std::string buffer = "M14\n";
        WHEN("parsing") {
            auto res =
                gcode::DeactivatePlate::parse(buffer.begin(), buffer.end());
            THEN("a valid gcode should be produced") {
                REQUIRE(res.first.has_value());
                REQUIRE(res.second != buffer.begin());
            }
        }
    }
    GIVEN("an invalid input") {
        std::string buffer = "M 108\n";
        WHEN("parsing") {
            auto res =
                gcode::DeactivatePlate::parse(buffer.begin(), buffer.end());
            THEN("an error should be produced") {
                REQUIRE(!res.first.has_value());
                REQUIRE(res.second == buffer.begin());
            }
        }
    }
}
