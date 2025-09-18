#include "catch2/catch.hpp"
#include "gcode_test_harness.hpp"

SCENARIO("GetErrorState (M411) parser works", "[gcode][parse][m411]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetErrorState::write_response_into(
                buffer.begin(), buffer.end(), true);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M411 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetErrorState::write_response_into(
                buffer.begin(), buffer.begin() + 7, true);
            THEN("the response should write only up to the available space") {
                std::string response = "M411cccccccccc";
                response[6] = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("valid input") {
        std::string input = "M411\n";
        WHEN("parsing input") {
            auto parsed =
                gcode::GetErrorState::parse(input.begin(), input.end());
            THEN("the gcode is parsed") {
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.second != input.begin());
            }
        }
    }
    GIVEN("invalid input") {
        std::string input = "M4asda\n";
        WHEN("parsing input") {
            auto parsed =
                gcode::GetErrorState::parse(input.begin(), input.end());
            THEN("the gcode is not parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == input.begin());
            }
        }
    }
}
