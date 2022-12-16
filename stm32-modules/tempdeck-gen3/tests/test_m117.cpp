#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "tempdeck-gen3/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetOffsetConstants (M117) parser works", "[gcode][parse][m117]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetOffsetConstants::write_response_into(
                buffer.begin(), buffer.end(), 0, 10.0, 15.0);
            THEN("the response should be written in full") {
                auto response_str = "M117 A:0.0000 B:10.0000 C:15.0000 OK\n";
                REQUIRE_THAT(buffer, Catch::Matchers::StartsWith(response_str));
                REQUIRE(written == buffer.begin() + strlen(response_str));
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetOffsetConstants::write_response_into(
                buffer.begin(), buffer.begin() + 7, 0, 10.0, 15.0);
            THEN("the response should write only up to the available space") {
                std::string response = "M117 Acccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a valid input") {
        std::string buffer = "M117\n";
        WHEN("parsing") {
            auto res =
                gcode::GetOffsetConstants::parse(buffer.begin(), buffer.end());
            THEN("a valid gcode should be produced") {
                REQUIRE(res.first.has_value());
                REQUIRE(res.second != buffer.begin());
            }
        }
    }
    GIVEN("an invalid input") {
        std::string buffer = "M 117\n";
        WHEN("parsing") {
            auto res =
                gcode::GetOffsetConstants::parse(buffer.begin(), buffer.end());
            THEN("an error should be produced") {
                REQUIRE(!res.first.has_value());
                REQUIRE(res.second == buffer.begin());
            }
        }
    }
}
