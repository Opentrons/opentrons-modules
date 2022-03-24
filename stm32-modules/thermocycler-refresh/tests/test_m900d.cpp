#include "catch2/catch.hpp"

// Push this diagnostic to avoid a compiler error about printing to too
// small of a buffer... which we're doing on purpose!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-refresh/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetBoardRevision (M900.D) parser works", "[gcode][parse][m900d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetBoardRevision::write_response_into(
                buffer.begin(), buffer.end(), 1);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M900.D C:1 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetBoardRevision::write_response_into(
                buffer.begin(), buffer.begin() + 7, 1);
            THEN("the response should write only up to the available space") {
                std::string response = "M900.Dcccccccccc";
                response[6] = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("valid input") {
        std::string input = "M900.D\n";
        WHEN("parsing input") {
            auto parsed =
                gcode::GetBoardRevision::parse(input.begin(), input.end());
            THEN("the gcode is parsed") {
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.second != input.begin());
            }
        }
    }
}
