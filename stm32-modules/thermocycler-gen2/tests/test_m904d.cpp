#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-gen2/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetLightsDebug (M904.D) parser works", "[gcode][parse][M904.d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::SetLightsDebug::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M904.D OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetLightsDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7);
            THEN("the response should write only up to the available space") {
                std::string response = "M904.D ccccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("valid input to enable fans") {
        std::string input = "M904.D S1\n";
        WHEN("parsing input") {
            auto result =
                gcode::SetLightsDebug::parse(input.begin(), input.end());
            THEN("parsing is succesful") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second != input.begin());
                REQUIRE(result.first.value().enable);
            }
        }
    }
    GIVEN("valid input to disable fans") {
        std::string input = "M904.D S0\n";
        WHEN("parsing input") {
            auto result =
                gcode::SetLightsDebug::parse(input.begin(), input.end());
            THEN("parsing is succesful") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second != input.begin());
                REQUIRE(!result.first.value().enable);
            }
        }
    }
    GIVEN("invalid input") {
        std::string input = GENERATE("M904.D S\n", "M904.D\n");
        WHEN("parsing input") {
            auto result =
                gcode::SetLightsDebug::parse(input.begin(), input.end());
            THEN("parsing is not succesful") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == input.begin());
            }
        }
    }
}
