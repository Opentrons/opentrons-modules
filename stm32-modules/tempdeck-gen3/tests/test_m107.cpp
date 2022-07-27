#include "catch2/catch.hpp"

// Push this diagnostic to avoid a compiler error about printing to too
// small of a buffer... which we're doing on purpose!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "tempdeck-gen3/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetFanAutomatic (M107) parser works", "[gcode][parse][m107]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::SetFanAutomatic::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M107 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetFanAutomatic::write_response_into(
                buffer.begin(), buffer.begin() + 5);
            THEN("the response should write only up to the available space") {
                std::string response = "M107 ccccccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a valid input") {
        std::string buffer = "M107\n";
        WHEN("parsing") {
            auto res =
                gcode::SetFanAutomatic::parse(buffer.begin(), buffer.end());
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
                gcode::SetFanAutomatic::parse(buffer.begin(), buffer.end());
            THEN("an error should be produced") {
                REQUIRE(!res.first.has_value());
                REQUIRE(res.second == buffer.begin());
            }
        }
    }
}
