#include <array>

#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "heater-shaker/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("ClosePlateLock (M243) response works", "[gcode][parse][M243]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(64, 'c');
        WHEN("filling response") {
            auto written = gcode::ClosePlateLock::write_response_into(
                buffer.begin(), buffer.end());
            THEN("the response should be written in full") {
                std::string ok = "M243 OK\n";
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(ok));
                REQUIRE(written == buffer.begin() + ok.size());
                std::string suffix(buffer.size() - ok.size(), 'c');
                REQUIRE_THAT(buffer, Catch::Matchers::EndsWith(suffix));
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::ClosePlateLock::write_response_into(
                buffer.begin(), buffer.begin() + 6);
            THEN("the response should write only up to the available space") {
                std::string response = "M243 Occcccccccc";
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written == buffer.begin() + 6);
            }
        }
    }
}
