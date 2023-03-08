#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "tempdeck-gen3/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetTemperatureDebug (M105.D) parser works",
         "[gcode][parse][m105.d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperatureDebug::write_response_into(
                buffer.begin(), buffer.end(), 10.0, 11.0, 15.0, 10, 11, 15);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M105.D PT1:10.00 PT2:11.00 HST:15.00 "
                                         "PA1:10 PA2:11 HSA:15 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperatureDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10.0, 11.0, 15.0, 10, 11,
                15);
            THEN("the response should write only up to the available space") {
                std::string response = "M105.Dcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}
