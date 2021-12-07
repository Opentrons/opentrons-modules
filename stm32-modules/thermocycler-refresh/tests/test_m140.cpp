#include "catch2/catch.hpp"
#include "thermocycler-refresh/gcodes.hpp"

SCENARIO("SetLidTemperature (M140) parser works", "[gcode][parse][m140]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetLidTemperature::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M140 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetLidTemperature::write_response_into(
                buffer.begin(), buffer.begin() + 6);
            THEN("the response should write only up to the available space") {
                std::string response = "M140 Occcccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("valid parameters") {
        WHEN("Setting target to 100C") {
            std::string buffer = "M140 S100\n";
            auto parsed =
                gcode::SetLidTemperature::parse(buffer.begin(), buffer.end());
            THEN("the target should be 100") {
                auto &val = parsed.first;
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.has_value());
                REQUIRE(val.value().setpoint == 100.0F);
            }
        }
        WHEN("Setting target to 0.0") {
            std::string buffer = "M140 S0.0\n";
            auto parsed =
                gcode::SetLidTemperature::parse(buffer.begin(), buffer.end());
            THEN("the target should be 0.0") {
                auto &val = parsed.first;
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.has_value());
                REQUIRE(val.value().setpoint == 0.0F);
            }
        }
    }
    GIVEN("invalid input") {
        std::string buffer = "M1 40 S 1 00\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetLidTemperature::parse(buffer.begin(), buffer.end());
            THEN("parsing fails") {
                REQUIRE(parsed.second == buffer.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
}
