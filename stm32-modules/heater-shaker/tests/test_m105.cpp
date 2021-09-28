#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetTemperature (M105) response works", "[gcode][response][m105]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperature::write_response_into(
                buffer.begin(), buffer.end(), 10.25, 25.001);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M105 C:10.25 T:25.00 OK\n"));
                REQUIRE(written == buffer.begin() + 24);
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetTemperature::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10, 25);
            THEN("the response should write only up to the available space") {
                std::string response = "M105 Ccccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}
