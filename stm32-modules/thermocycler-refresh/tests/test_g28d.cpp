#include "catch2/catch.hpp"
#include "thermocycler-refresh/gcodes.hpp"

SCENARIO("gcode g28.d works", "[gcode][parse][g28d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::ActuateSolenoid::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("G28.D OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::ActuateSolenoid::write_response_into(
                buffer.begin(), buffer.begin() + 7);
            THEN("the response should write only up to the available space") {
                std::string response = "G28.D Occccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("command to turn on solenoid") {
        std::string buffer = "G28.D 1\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::ActuateSolenoid::parse(buffer.begin(), buffer.end());
            THEN("engage should be true") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.first.value().engage);
            }
        }
    }

    GIVEN("command to turn off solenoid") {
        std::string buffer = "G28.D 0\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::ActuateSolenoid::parse(buffer.begin(), buffer.end());
            THEN("engage should be false") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE(!parsed.first.value().engage);
            }
        }
    }
    GIVEN("invalid input") {
        std::string buffer = "G28.D hello";
        WHEN("parsing the command") {
            auto parsed =
                gcode::ActuateSolenoid::parse(buffer.begin(), buffer.end());
            THEN("parsing should fail") {
                REQUIRE(parsed.second == buffer.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
}
