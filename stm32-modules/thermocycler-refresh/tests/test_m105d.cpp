#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-refresh/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetPlateTemperatureDebug (M105.D) parser works",
         "[gcode][parse][m105.d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetPlateTemperatureDebug::write_response_into(
                buffer.begin(), buffer.end(), 10.0, 15.0, 20.0, 25.0, 30.0,
                35.0, 40.0, 10, 15, 20, 25, 30, 35, 40);
            THEN("the response should be written in full") {
                REQUIRE_THAT(
                    buffer,
                    Catch::Matchers::StartsWith(
                        "M105.D HST:10.00 FRT:15.00 FLT:20.00 FCT:25.00 "
                        "BRT:30.00 BLT:35.00 BCT:40.00 HSA:10 FRA:15 FLA:20 "
                        "FCA:25 BRA:30 BLA:35 BCA:40 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetPlateTemperatureDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10.0, 15.0, 20.0, 25.0,
                30.0, 35.0, 40.0, 10, 15, 20, 25, 30, 35, 40);
            THEN("the response should write only up to the available space") {
                std::string response = "M105.Dcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}
