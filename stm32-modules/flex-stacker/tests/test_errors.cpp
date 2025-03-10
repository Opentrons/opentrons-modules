#include <array>
#include <cstring>

#include "catch2/catch.hpp"
#include "flex-stacker/errors.hpp"

SCENARIO("testing error writing") {
    GIVEN("an error code and a buffer to write into") {
        auto error_code = errors::ErrorCode::UNEXPECTED_LIMIT_SWITCH;
        auto buffer = std::string(64, ' ');

        WHEN("writing as a sync error") {
            auto written =
                errors::write_into(buffer.begin(), buffer.end(), error_code);
            THEN("the error is written into the buffer") {
                auto expected =
                    std::string(errors::errorstring(error_code)) + " OK\n";
                REQUIRE_THAT(buffer, Catch::Matchers::Contains(expected));
                AND_THEN("the length was appropriately returned") {
                    REQUIRE(written == buffer.begin() + expected.length());
                }
            }
        }

        WHEN("writing as an async error") {
            auto written = errors::write_into_async(buffer.begin(),
                                                    buffer.end(), error_code);
            THEN("the error is written into the buffer") {
                auto expected = "async " +
                                std::string(errors::errorstring(error_code)) +
                                "\n";
                REQUIRE_THAT(buffer, Catch::Matchers::Contains(expected));
                AND_THEN("the length was appropriately returned") {
                    REQUIRE(written == buffer.begin() + expected.length());
                }
            }
        }
    }
}