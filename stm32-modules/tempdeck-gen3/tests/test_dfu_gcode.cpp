
#include "catch2/catch.hpp"
#include "systemwide.h"
#include "tempdeck-gen3/errors.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include "tempdeck-gen3/gcodes.hpp"
#pragma GCC diagnostic pop

SCENARIO("dfu parser works") {
    GIVEN("an empty string") {
        std::string to_parse = "";

        WHEN("calling parse") {
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a fully non-matching string") {
        std::string to_parse = "asdhalghasdasd ";

        WHEN("calling parse") {
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a subprefix matching only") {
        std::string to_parse = "dfasdlasfhalsd\r\n";
        WHEN("calling parse") {
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("nothing should be parsed") {
                REQUIRE(!result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin());
            }
        }
    }

    GIVEN("a string with a good gcode") {
        auto to_parse = std::string("dfu\r\n");

        WHEN("calling parse") {
            auto result = gcode::EnterBootloader::parse(to_parse.cbegin(),
                                                        to_parse.cend());
            THEN("a gcode should be parsed") {
                REQUIRE(result.first.has_value());
                REQUIRE(result.second == to_parse.cbegin() + 3);
            }
        }
    }

    GIVEN("a response buffer large enough for the response") {
        std::string response_buf(64, 'c');
        WHEN("filling the response") {
            auto written = gcode::EnterBootloader::write_response_into(
                response_buf.begin(), response_buf.end());
            THEN("the response should be filled") {
                REQUIRE_THAT(response_buf,
                             Catch::Matchers::StartsWith("dfu OK\n"));
                REQUIRE(written == response_buf.begin() + 7);
            }
        }
    }

    GIVEN("a response buffer not large enough for the response") {
        std::string response_buf(10, 'c');
        auto written = gcode::EnterBootloader::write_response_into(
            response_buf.begin(), response_buf.begin() + 3);
        THEN("the response should be filled only up to its size") {
            REQUIRE_THAT(response_buf, Catch::Matchers::Equals("dfuccccccc"));
            REQUIRE(written == response_buf.begin() + 3);
        }
    }
}
