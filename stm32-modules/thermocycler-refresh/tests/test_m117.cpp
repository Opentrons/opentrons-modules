#include "catch2/catch.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "thermocycler-refresh/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("GetOffsetConstants (M105.D) parser works",
         "[gcode][parse][m105.d]") {
    GIVEN("a response buffer large enough for the formatted response") {
        std::string buffer(256, 'c');
        WHEN("filling response") {
            auto written = gcode::GetOffsetConstants::write_response_into(
                buffer.begin(), buffer.end(), 10.0, 15.0);
            THEN("the response should be written in full") {
                auto response_str = "M117 B:10.00 C:15.00 OK\n";
                REQUIRE_THAT(
                    buffer,
                    Catch::Matchers::StartsWith(
                        response_str));
                REQUIRE(written == buffer.begin() + strlen(response_str));
            }
        }
    }

    GIVEN("a response buffer not large enough for the formatted response") {
        std::string buffer(16, 'c');
        WHEN("filling response") {
            auto written = gcode::GetOffsetConstants::write_response_into(
                buffer.begin(), buffer.begin() + 7, 10.0, 15.0);
            THEN("the response should write only up to the available space") {
                std::string response = "M117 Bcccccccccc";
                response.at(6) = '\0';
                REQUIRE_THAT(buffer, Catch::Matchers::Equals(response));
                REQUIRE(written != buffer.begin());
            }
        }
    }
    GIVEN("a valid input") {
        std::string buffer = "M117\n";
        WHEN("parsing") {
            auto res = gcode::GetOffsetConstants::parse(buffer.begin(), buffer.end());
            THEN("a valid gcode should be produced") {
                REQUIRE(res.first.has_value());
                REQUIRE(res.second != buffer.begin());
            }
        }
    }
    GIVEN("an invalid input") {
        std::string buffer = "M 117\n";
        WHEN("parsing") {
            auto res = gcode::GetOffsetConstants::parse(buffer.begin(), buffer.end());
            THEN("an error should be produced") {
                REQUIRE(!res.first.has_value());
                REQUIRE(res.second == buffer.begin());
            }
        }
    }
}
