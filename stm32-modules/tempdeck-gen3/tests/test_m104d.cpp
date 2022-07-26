#include "catch2/catch.hpp"
#include "tempdeck-gen3/gcodes.hpp"

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
        WHEN("Setting peltier to a positive power") {
            std::string buffer = "M104.D S1\n";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("the input is parsed succesfully") {
                auto &val = parsed.first;
                REQUIRE(val.has_value());
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.value().power == 1.0f);
            }
        }
        WHEN("Setting peltier to a negative power") {
            std::string buffer = "M104.D S-0.5\n";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("the input is parsed succesfully") {
                auto &val = parsed.first;
                REQUIRE(val.has_value());
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE_THAT(val.value().power,
                             Catch::Matchers::WithinAbs(-0.5, 0.001));
            }
        }
    }
    GIVEN("Invalid parameters") {
        WHEN("no power is provided") {
            std::string buffer = "M104.D S\n";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
        WHEN("no argument is provided") {
            std::string buffer = "M104.D    \n";
            auto parsed =
                gcode::SetPeltierDebug::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
    }
}
