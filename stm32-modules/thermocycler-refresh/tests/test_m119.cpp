#include "catch2/catch.hpp"

// Push this diagnostic to avoid a compiler error about printing to too
// small of a buffer... which we're doing on purpose!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-refresh/gcodes.hpp"
#pragma GCC diagnostic pop

#include "thermocycler-refresh/motor_utils.hpp"

SCENARIO("GetLidStatus (M119) parser works", "[gcode][parse][m119]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto lid = motor_util::LidStepper::Status::UNKNOWN;
            auto seal = motor_util::SealStepper::Status::UNKNOWN;
            auto written = gcode::GetLidStatus::write_response_into(
                buffer.begin(), buffer.end(), lid, seal);
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(
                                         "M119 Lid:unknown Seal:unknown OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a valid input") {
        std::string buffer = "M119\n";
        WHEN("parsing") {
            auto res = gcode::GetLidStatus::parse(buffer.begin(), buffer.end());
            THEN("a valid gcode should be produced") {
                REQUIRE(res.first.has_value());
                REQUIRE(res.second != buffer.begin());
            }
        }
    }
    GIVEN("an invalid input") {
        std::string buffer = "M 119\n";
        WHEN("parsing") {
            auto res = gcode::GetLidStatus::parse(buffer.begin(), buffer.end());
            THEN("an error should be produced") {
                REQUIRE(!res.first.has_value());
                REQUIRE(res.second == buffer.begin());
            }
        }
    }
}
