#include "catch2/catch.hpp"

// Push this diagnostic to avoid a compiler error about printing to too
// small of a buffer... which we're doing on purpose!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-gen2/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetLidSwitches (M901.D) parser works", "[gcode][parse][m901d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetLidSwitches::write_response_into(
                buffer.begin(), buffer.end(), true, true, true, false);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M901.D C:1 O:1 E:1 R:0 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetLidSwitches::write_response_into(
                buffer.begin(), buffer.begin() + 7, true, true, true, true);
            THEN("the response should write only up to the available space") {
                std::string response = "M901.Dcccccccccc";
                response[6] = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("valid input") {
        std::string input = "M901.D\n";
        WHEN("parsing input") {
            auto parsed =
                gcode::GetLidSwitches::parse(input.begin(), input.end());
            THEN("the gcode is parsed") {
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.second != input.begin());
            }
        }
    }
}
