#include "catch2/catch.hpp"
#include "thermocycler-refresh/gcodes.hpp"

SCENARIO("gcode m240.d works", "[gcode][parse][m240d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::ActuateLidStepperDebug::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M240.D OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::ActuateLidStepperDebug::write_response_into(
                buffer.begin(), buffer.begin() + 8);
            THEN("the response should write only up to the available space") {
                std::string response = "M240.D Occcccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("command to move motor 20 degrees") {
        std::string buffer = "M240.D 20\n";
        WHEN("parsing the command") {
            auto parsed = gcode::ActuateLidStepperDebug::parse(buffer.begin(),
                                                               buffer.end());
            THEN("the angle should be correct") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE_THAT(parsed.first.value().angle,
                             Catch::Matchers::WithinAbs(20, 0.1));
                REQUIRE(!parsed.first.value().overdrive);
            }
        }
    }

    GIVEN("command to move motor -20.5 degrees with overdrive") {
        std::string buffer = "M240.D -20.5 O\n";
        WHEN("parsing the command") {
            auto parsed = gcode::ActuateLidStepperDebug::parse(buffer.begin(),
                                                               buffer.end());
            THEN("the angle should be correct") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE_THAT(parsed.first.value().angle,
                             Catch::Matchers::WithinAbs(-20.5, 0.1));
                REQUIRE(parsed.first.value().overdrive);
            }
        }
    }
    GIVEN("invalid input") {
        std::string buffer = "M240.D hello\n";
        WHEN("parsing the command") {
            auto parsed = gcode::ActuateLidStepperDebug::parse(buffer.begin(),
                                                               buffer.end());
            THEN("parsing should fail") {
                REQUIRE(parsed.second == buffer.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
}
