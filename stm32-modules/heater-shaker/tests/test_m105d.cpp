#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetTemperatureDebug (M105.D) parser works",
         "[gcode][parse][m105.d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperatureDebug::write_response_into(
                buffer.begin(), buffer.end(), 10.25, 11.25, 12.25, 10, 11, 12,
                true);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M105.D AT:10.25 BT:11.25 OT:12.25 AD:10 "
                                         "BD:11 OD:12 PG:1 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperatureDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10.01, 11.2, 41.2, 44, 10,
                4, false);
            THEN("the response should write only up to the available space") {
                std::string response = "M105.Dcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}
