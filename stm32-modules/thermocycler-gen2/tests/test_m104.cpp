#include "catch2/catch.hpp"
#include "thermocycler-gen2/gcodes.hpp"

SCENARIO("SetPlateTemperature (M104) parser works", "[gcode][parse][m104]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetPlateTemperature::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M104 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetPlateTemperature::write_response_into(
                buffer.begin(), buffer.begin() + 6);
            THEN("the response should write only up to the available space") {
                std::string response = "M104 Occcccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("valid parameters") {
        WHEN("Setting target to 95C") {
            std::string buffer = "M104 S95\n";
            auto parsed =
                gcode::SetPlateTemperature::parse(buffer.begin(), buffer.end());
            THEN("the target should be 100") {
                auto &val = parsed.first;
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.has_value());
                REQUIRE(val.value().setpoint == 95.0F);
                REQUIRE(val.value().hold_time ==
                        gcode::SetPlateTemperature::infinite_hold);
            }
        }
        WHEN("Setting target to 0.0") {
            std::string buffer = "M104 S0.0\n";
            auto parsed =
                gcode::SetPlateTemperature::parse(buffer.begin(), buffer.end());
            THEN("the target should be 0.0") {
                auto &val = parsed.first;
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.has_value());
                REQUIRE(val.value().setpoint == 0.0F);
                REQUIRE(val.value().hold_time ==
                        gcode::SetPlateTemperature::infinite_hold);
            }
        }
        WHEN("Setting target to 50C with a hold time of 40 seconds") {
            std::string buffer = "M104 S50.0 H40\n";
            auto parsed =
                gcode::SetPlateTemperature::parse(buffer.begin(), buffer.end());
            THEN("the target should be 50.0") {
                auto &val = parsed.first;
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.has_value());
                REQUIRE(val.value().setpoint == 50.0F);
                REQUIRE(val.value().hold_time == 40.0F);
            }
        }
    }
    GIVEN("invalid input") {
        std::string buffer = "M104\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetPlateTemperature::parse(buffer.begin(), buffer.end());
            THEN("parsing fails") {
                REQUIRE(parsed.second == buffer.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
    GIVEN("wrong gcode") {
        std::string buffer = "M1044 S\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetPlateTemperature::parse(buffer.begin(), buffer.end());
            THEN("parsing fails") {
                REQUIRE(parsed.second == buffer.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
}
