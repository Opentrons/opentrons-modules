#include <utility>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "thermocycler-gen2/gcodes.hpp"

SCENARIO("SetOffsetConstants (M116) parser works", "[gcode][parse][m116]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetOffsetConstants::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith("M116 OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetOffsetConstants::write_response_into(
                buffer.begin(), buffer.begin() + 6);
            THEN("the response should write only up to the available space") {
                std::string response = "M116 Occcccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("input to set no constants") {
        using TestCaseType = std::pair<std::string, PeltierSelection>;
        const auto cases = GENERATE(
            as<TestCaseType>{},
            std::make_pair(std::string("M116\n"), PeltierSelection::ALL),
            std::make_pair(std::string("M116.L\n"), PeltierSelection::LEFT),
            std::make_pair(std::string("M116.R\n"), PeltierSelection::RIGHT),
            std::make_pair(std::string("M116.C\n"), PeltierSelection::CENTER),
            std::make_pair(std::string("M116.L \n"), PeltierSelection::LEFT));
        WHEN("parsing") {
            auto input = cases.first;
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(!val.const_a.defined);
                REQUIRE(!val.const_b.defined);
                REQUIRE(!val.const_c.defined);
                REQUIRE(val.channel == cases.second);
            }
        }
    }
    GIVEN("input to set B constant") {
        std::string input = "M116.L B-0.543\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(val.channel == PeltierSelection::LEFT);
                REQUIRE(!val.const_a.defined);
                REQUIRE(val.const_b.defined);
                REQUIRE_THAT(val.const_b.value,
                             Catch::Matchers::WithinAbs(-0.543, 0.01));
                REQUIRE(!val.const_c.defined);
            }
        }
    }
    GIVEN("input to set C constant") {
        std::string input = "M116.C C123.5\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(val.channel == PeltierSelection::CENTER);
                REQUIRE(!val.const_a.defined);
                REQUIRE(!val.const_b.defined);
                REQUIRE(val.const_c.defined);
                REQUIRE_THAT(val.const_c.value,
                             Catch::Matchers::WithinAbs(123.5, 0.01));
            }
        }
    }
    GIVEN("input to set A constant") {
        std::string input = "M116 A123.5\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(val.channel == PeltierSelection::ALL);
                REQUIRE(!val.const_b.defined);
                REQUIRE(!val.const_c.defined);
                REQUIRE(val.const_a.defined);
                REQUIRE_THAT(val.const_a.value,
                             Catch::Matchers::WithinAbs(123.5, 0.01));
            }
        }
    }
    GIVEN("input to set B and C constants") {
        std::string input = "M116 B543 C123.5\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(!val.const_a.defined);
                REQUIRE(val.const_b.defined);
                REQUIRE_THAT(val.const_b.value,
                             Catch::Matchers::WithinAbs(543, 0.01));
                REQUIRE(val.const_c.defined);
                REQUIRE_THAT(val.const_c.value,
                             Catch::Matchers::WithinAbs(123.5, 0.01));
            }
        }
    }
    GIVEN("input to set A, B, and C constants") {
        std::string input = "M116 A2.043 B543 C123.5\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(val.const_a.defined);
                REQUIRE_THAT(val.const_a.value,
                             Catch::Matchers::WithinAbs(2.043, 0.001));
                REQUIRE(val.const_b.defined);
                REQUIRE_THAT(val.const_b.value,
                             Catch::Matchers::WithinAbs(543, 0.001));
                REQUIRE(val.const_c.defined);
                REQUIRE_THAT(val.const_c.value,
                             Catch::Matchers::WithinAbs(123.5, 0.001));
            }
        }
    }
    GIVEN("invalid input") {
        std::string input = "M1116\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should not be succesful") {
                REQUIRE(parsed.second == input.begin());
                REQUIRE(!parsed.first.has_value());
            }
        }
    }
}
