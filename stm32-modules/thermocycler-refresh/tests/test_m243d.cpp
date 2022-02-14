#include "catch2/catch.hpp"
#include "thermocycler-refresh/gcodes.hpp"

SCENARIO("gcode m243.d parameter utility function works") {
    GIVEN("invalid parameter") {
        std::array<char, 4> inputs = {'a', 'B', 'r', 'w'};
        WHEN("checking for validity") {
            for (auto c : inputs) {
                THEN("no match occurs") {
                    REQUIRE(!gcode::SetSealParameter::is_legal_parameter(c));
                }
            }
        }
    }
    GIVEN("valid parameter") {
        std::array<char, 6> inputs = {'V', 'A', 'T', 'M', 'R', 'H'};
        WHEN("checking for validity") {
            for (auto c : inputs) {
                THEN("match occurs") {
                    REQUIRE(gcode::SetSealParameter::is_legal_parameter(c));
                }
            }
        }
    }
}

SCENARIO("gcode m243.d works", "[gcode][parse][m243d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetSealParameter::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M243.D OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetSealParameter::write_response_into(
                buffer.begin(), buffer.begin() + 8);
            THEN("the response should write only up to the available space") {
                std::string response = "M243.D Occcccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("command to set velocity") {
        std::string buffer = "M243.D V 10000\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetSealParameter::parse(buffer.begin(), buffer.end());
            THEN("the result should be correct") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.first.value().parameter ==
                        gcode::SetSealParameter::SealParameter::Velocity);
                REQUIRE(parsed.first.value().value == 10000);
            }
        }
    }
    GIVEN("command to set acceleration") {
        std::string buffer = "M243.D A 40\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetSealParameter::parse(buffer.begin(), buffer.end());
            THEN("the result should be correct") {
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.first.value().parameter ==
                        gcode::SetSealParameter::SealParameter::Acceleration);
                REQUIRE(parsed.first.value().value == 40);
            }
        }
    }
}
