#include "catch2/catch.hpp"
#include "tempdeck-gen3/gcodes.hpp"

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
        std::string input = "M116\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(!val.const_a.has_value());
                REQUIRE(!val.const_b.has_value());
                REQUIRE(!val.const_c.has_value());
            }
        }
    }
    GIVEN("input to set B constant") {
        std::string input = "M116 B-0.543\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(!val.const_a.has_value());
                REQUIRE(val.const_b.has_value());
                REQUIRE_THAT(val.const_b.value(),
                             Catch::Matchers::WithinAbs(-0.543, 0.01));
                REQUIRE(!val.const_c.has_value());
            }
        }
    }
    GIVEN("input to set C constant") {
        std::string input = "M116 C123.5\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(!val.const_a.has_value());
                REQUIRE(!val.const_b.has_value());
                REQUIRE(val.const_c.has_value());
                REQUIRE_THAT(val.const_c.value(),
                             Catch::Matchers::WithinAbs(123.5, 0.01));
            }
        }
    }
    GIVEN("input to set all constants") {
        std::string input = "M116 A-3 B543 C123.5\n";
        WHEN("parsing") {
            auto parsed =
                gcode::SetOffsetConstants::parse(input.begin(), input.end());
            THEN("parsing should be succesful") {
                REQUIRE(parsed.second != input.begin());
                REQUIRE(parsed.first.has_value());
                auto &val = parsed.first.value();
                REQUIRE(val.const_a.has_value());
                REQUIRE_THAT(val.const_a.value(),
                             Catch::Matchers::WithinAbs(-3, 0.01));
                REQUIRE(val.const_b.has_value());
                REQUIRE_THAT(val.const_b.value(),
                             Catch::Matchers::WithinAbs(543, 0.01));
                REQUIRE(val.const_c.has_value());
                REQUIRE_THAT(val.const_c.value(),
                             Catch::Matchers::WithinAbs(123.5, 0.01));
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
