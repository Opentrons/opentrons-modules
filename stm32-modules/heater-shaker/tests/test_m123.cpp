#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetRPM (M103) response generation works", "[gcode][response][m103]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::GetRPM::write_response_into(
                buffer.begin(), buffer.end(), 10, 25);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M123 C10 T25 OK\n"));
                REQUIRE(written == buffer.begin() + 16);
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetRPM::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10, 25);
            THEN("the response should write only up to the available space") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::Equals("M123 Ccccccccccc"));
                REQUIRE(written == buffer.begin() + 7);
            }
        }
    }
}
