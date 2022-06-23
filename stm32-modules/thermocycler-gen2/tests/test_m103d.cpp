#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-gen2/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetThermalPowerDebug (M103.D) parser works",
         "[gocde][parse][m103.d]") {
    GIVEN("a response buffer large enough for formatted response") {
        std::string buffer(256, 'c');
        WHEN("writing response") {
            auto written = gcode::GetThermalPowerDebug::write_response_into(
                buffer.begin(), buffer.end(), 0.0, 0.1, 0.2, 0.3, 0.4, 0.5,
                0.6);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M103.D L:0.00 C:0.10 R:0.20 H:0.30 "
                                         "F:0.40 T1:0.50 T2:0.60 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetThermalPowerDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7, 0.0, 0.1, 0.2, 0.3, 0.4,
                0.5, 0.6);
            THEN("the response should write only up to the available space") {
                std::string response = "M103.Dcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("correct input") {
        std::string input("M103.D\n");
        WHEN("parsing the command") {
            auto parsed =
                gcode::GetThermalPowerDebug::parse(input.begin(), input.end());
            THEN("the command should be correct") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
            }
        }
    }
    GIVEN("incorrect input") {
        std::string input("M103.E \n");
        WHEN("parsing the command") {
            auto parsed =
                gcode::GetThermalPowerDebug::parse(input.begin(), input.end());
            THEN("the command should be incrorrect") {
                REQUIRE(parsed.second == input.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
}
