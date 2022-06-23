#include "catch2/catch.hpp"
#include "systemwide.h"
// Push this diagnostic to avoid a compiler error about printing to too
// small of a buffer... which we're doing on purpose!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-gen2/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("SetPIDConstants (M301) parser works", "[gcode][parse][m301]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::SetPIDConstants::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M301 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetPIDConstants::write_response_into(
                buffer.begin(), buffer.begin() + 6);
            THEN("the response should write only up to the available space") {
                std::string response = "M301 Occcccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("valid input without a target specifier") {
        std::string buffer = "M301 P10.0 I-4 D75\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetPIDConstants::parse(buffer.begin(), buffer.end());
            THEN("a valid command is produced") {
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.second != buffer.begin());
                auto val = parsed.first.value();
                REQUIRE(val.const_p == 10.0F);
                REQUIRE(val.const_i == -4.0F);
                REQUIRE(val.const_d == 75.0F);
                REQUIRE(val.selection == PidSelection::PELTIERS);
            }
        }
    }
    GIVEN("valid input specifying the peltiers") {
        std::string buffer = "M301 SP P10.0 I-4 D75\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetPIDConstants::parse(buffer.begin(), buffer.end());
            THEN("a valid command is produced") {
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.second != buffer.begin());
                auto val = parsed.first.value();
                REQUIRE(val.const_p == 10.0F);
                REQUIRE(val.const_i == -4.0F);
                REQUIRE(val.const_d == 75.0F);
                REQUIRE(val.selection == PidSelection::PELTIERS);
            }
        }
    }
    GIVEN("valid input specifying the fans") {
        std::string buffer = "M301 SF P10.0 I-4 D75\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetPIDConstants::parse(buffer.begin(), buffer.end());
            THEN("a valid command is produced") {
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.second != buffer.begin());
                auto val = parsed.first.value();
                REQUIRE(val.const_p == 10.0F);
                REQUIRE(val.const_i == -4.0F);
                REQUIRE(val.const_d == 75.0F);
                REQUIRE(val.selection == PidSelection::FANS);
            }
        }
    }
    GIVEN("valid input specifying the heater") {
        std::string buffer = "M301 SH P10.0 I-4 D75\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetPIDConstants::parse(buffer.begin(), buffer.end());
            THEN("a valid command is produced") {
                REQUIRE(parsed.first.has_value());
                REQUIRE(parsed.second != buffer.begin());
                auto val = parsed.first.value();
                REQUIRE(val.const_p == 10.0F);
                REQUIRE(val.const_i == -4.0F);
                REQUIRE(val.const_d == 75.0F);
                REQUIRE(val.selection == PidSelection::HEATER);
            }
        }
    }
    GIVEN("input with invalid target specifier") {
        std::string buffer = "M301 SW P10.0 I-4 D75\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetPIDConstants::parse(buffer.begin(), buffer.end());
            THEN("no valid command is produced") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
    }
    GIVEN("invalid input") {
        std::string buffer = "M301 Px IW ABCDEFG\n";
        WHEN("parsing the command") {
            auto parsed =
                gcode::SetPIDConstants::parse(buffer.begin(), buffer.end());
            THEN("no valid command is produced") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
    }
}
