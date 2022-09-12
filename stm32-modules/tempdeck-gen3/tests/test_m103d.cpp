#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "tempdeck-gen3/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetThermalPowerDebug (M103.D) parser works",
         "[gcode][parse][m103.d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetThermalPowerDebug::write_response_into(
                buffer.begin(), buffer.end(), 10.0, 10, 15);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith(
                                 "M103.D I:10.000 P:10.000 F:15.000 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetThermalPowerDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10.0, 10, 15);
            THEN("the response should write only up to the available space") {
                std::string response = "M103.Dcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}
