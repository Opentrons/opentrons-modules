#include "catch2/catch.hpp"
#include "tempdeck-gen3/gcodes.hpp"

SCENARIO("SetTemperature (M104) parser works", "[gcode][parse][m104]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::SetTemperature::write_response_into(
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
            auto written = gcode::SetTemperature::write_response_into(
                buffer.begin(), buffer.begin() + 5);
            THEN("the response should write only up to the available space") {
                std::string response = "M104 ccccccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }

    GIVEN("valid parameters") {
        WHEN("Setting peltier to a positive target") {
            std::string buffer = "M104 S100\n";
            auto parsed =
                gcode::SetTemperature::parse(buffer.begin(), buffer.end());
            THEN("the input is parsed succesfully") {
                auto &val = parsed.first;
                REQUIRE(val.has_value());
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE(val.value().target == 100.0f);
            }
        }
        WHEN("Setting peltier to a negative target") {
            std::string buffer = "M104 S-5.5\n";
            auto parsed =
                gcode::SetTemperature::parse(buffer.begin(), buffer.end());
            THEN("the input is parsed succesfully") {
                auto &val = parsed.first;
                REQUIRE(val.has_value());
                REQUIRE(parsed.second != buffer.begin());
                REQUIRE_THAT(val.value().target,
                             Catch::Matchers::WithinAbs(-5.5, 0.001));
            }
        }
    }
    GIVEN("Invalid parameters") {
        WHEN("no target is provided") {
            std::string buffer = "M104 S\n";
            auto parsed =
                gcode::SetTemperature::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
        WHEN("no argument is provided") {
            std::string buffer = "M104    \n";
            auto parsed =
                gcode::SetTemperature::parse(buffer.begin(), buffer.end());
            THEN("Nothing should be parsed") {
                REQUIRE(!parsed.first.has_value());
                REQUIRE(parsed.second == buffer.begin());
            }
        }
    }
}
