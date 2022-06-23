#include "catch2/catch.hpp"
#include "thermocycler-gen2/gcodes.hpp"

SCENARIO("SetFanManual (M106) parser works", "[gcode][parse][m106]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetFanManual::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M106 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetFanManual::write_response_into(
                buffer.begin(), buffer.begin() + 3);
            THEN("the response should write only up to the available space") {
                std::string response = "M10ccccccccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("Valid parameters") {
        WHEN("Setting power to 1.0") {
            std::string buffer = "M106 S1.0\n";
            auto parsed =
                gcode::SetFanManual::parse(buffer.begin(), buffer.end());
            THEN("the power should be 1.0") {
                auto &val = parsed.first;
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.has_value());
                REQUIRE(val.value().power == 1.0F);
            }
        }
        WHEN("Setting power to 0.0") {
            std::string buffer = "M106 S0\n";
            auto parsed =
                gcode::SetFanManual::parse(buffer.begin(), buffer.end());
            THEN("the power should be 0.0") {
                auto &val = parsed.first;
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.has_value());
                REQUIRE(val.value().power == 0.0F);
            }
        }
    }
    GIVEN("Invalid power") {
        WHEN("Setting power to 2.0") {
            std::string buffer = "M106 S2.0\n";
            auto parsed =
                gcode::SetFanManual::parse(buffer.begin(), buffer.end());
            THEN("parsing should fail") {
                auto &val = parsed.first;
                REQUIRE(!val.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
    }
}
