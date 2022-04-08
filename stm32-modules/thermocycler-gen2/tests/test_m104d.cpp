#include "catch2/catch.hpp"
#include "thermocycler-gen2/gcodes.hpp"

SCENARIO("SetPeltierDebug (M104.D) parser works", "[gcode][parse][m104.d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetPeltierDebug::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith("M104.D OK\n"));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::SetPeltierDebug::write_response_into(
                buffer.begin(), buffer.begin() + 7);
            THEN("the response should write only up to the available space") {
                std::string response = "M104.D ccccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("valid parameters") {
        WHEN("Setting left peltier") {
            std::string buffer = "M104.D L P1.0 H";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("the left peltier should be selected") {
                auto &val = parsed.first;
                REQUIRE(val.has_value());
                REQUIRE(parsed.second == buffer.end());
                REQUIRE(val.value().power == 1.0f);
                REQUIRE(val.value().direction ==
                        PeltierDirection::PELTIER_HEATING);
                REQUIRE(val.value().peltier_selection ==
                        PeltierSelection::LEFT);
            }
        }
        WHEN("Setting right peltier") {
            std::string buffer = "M104.D R P1.0 C";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("the right peltier should be selected") {
                auto &val = parsed.first;
                REQUIRE(val.has_value());
                REQUIRE(parsed.second == buffer.end());
                REQUIRE(val.value().power == 1.0f);
                REQUIRE(val.value().direction ==
                        PeltierDirection::PELTIER_COOLING);
                REQUIRE(val.value().peltier_selection ==
                        PeltierSelection::RIGHT);
            }
        }
        WHEN("Setting center peltier") {
            std::string buffer = "M104.D C P1.0 H";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("the center peltier should be selected") {
                auto &val = parsed.first;
                REQUIRE(parsed.second == buffer.end());
                REQUIRE(val.has_value());
                REQUIRE(val.value().power == 1.0f);
                REQUIRE(val.value().direction ==
                        PeltierDirection::PELTIER_HEATING);
                REQUIRE(val.value().peltier_selection ==
                        PeltierSelection::CENTER);
            }
        }
        WHEN("Setting all peltiers") {
            std::string buffer = "M104.D A P1.0 C";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("all peltiers should be selected") {
                auto &val = parsed.first;
                REQUIRE(parsed.second == buffer.end());
                REQUIRE(val.has_value());
                REQUIRE(val.value().power == 1.0f);
                REQUIRE(val.value().direction ==
                        PeltierDirection::PELTIER_COOLING);
                REQUIRE(val.value().peltier_selection == PeltierSelection::ALL);
            }
        }
    }
    GIVEN("Invalid parameters") {
        WHEN("setting invalid peltier") {
            std::string buffer = "M104.D D P1.0 C";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
        WHEN("setting power too low") {
            std::string buffer = "M104.D A P-1.0 C";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
        WHEN("setting power too high") {
            std::string buffer = "M104.D A P1.5 C";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
        WHEN("setting invalid direction") {
            std::string buffer = "M104.D A P0.5 W";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
    }
}
