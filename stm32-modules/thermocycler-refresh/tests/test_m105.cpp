#include "catch2/catch.hpp"

// Push this diagnostic to avoid a compiler error about printing to too
// small of a buffer... which we're doing on purpose!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-refresh/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetPlateTemperature (M105) parser works",
         "[gcode][parse][M105]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetPlateTemp::write_response_into(
                buffer.begin(), buffer.end(), 10.0, 40);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M105 T:40.00 C:10.00 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
        WHEN("filling response with no target temp") {
            auto written = gcode::GetPlateTemp::write_response_into(
                buffer.begin(), buffer.end(), 10.0, 0);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M105 T:none C:10.00 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetPlateTemp::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10.0, 40);
            THEN("the response should write only up to the available space") {
                std::string response = "M105 Tcccccccccc";
                response[6] = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
}
