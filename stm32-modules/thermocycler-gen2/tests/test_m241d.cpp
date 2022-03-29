#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-gen2/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("gcode m241.d works", "[gcode][parse][m241d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::ActuateSealStepperDebug::write_response_into(
                buffer.begin(), buffer.end(), -1000);
            THEN("the response should be written in full") {
                REQUIRE_THAT(
                    buffer, Catch::Matchers::StartsWith("M241.D S:-1000 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::ActuateSealStepperDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7, -1000);
            THEN("the response should write only up to the available space") {
                std::string response = "M241.Dcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("command to move motor 20 steps") {
        std::string buffer = "M241.D 20\n";
        WHEN("parsing the command") {
            auto parsed = gcode::ActuateSealStepperDebug::parse(buffer.begin(),
                                                                buffer.end());
            THEN("the distance should be correct") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.first.value().distance == 20);
            }
        }
    }

    GIVEN("command to move motor -80000 steps") {
        std::string buffer = "M241.D -80000\n";
        WHEN("parsing the command") {
            auto parsed = gcode::ActuateSealStepperDebug::parse(buffer.begin(),
                                                                buffer.end());
            THEN("the distance should be correct") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.first.value().distance == -80000);
            }
        }
    }
    GIVEN("invalid input") {
        std::string buffer = "M241.D hello\n";
        WHEN("parsing the command") {
            auto parsed = gcode::ActuateSealStepperDebug::parse(buffer.begin(),
                                                                buffer.end());
            THEN("parsing should fail") {
                REQUIRE(parsed.second == buffer.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
}
