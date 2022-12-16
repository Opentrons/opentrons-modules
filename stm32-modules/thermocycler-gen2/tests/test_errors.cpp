#include <array>
#include <cstring>

#include "catch2/catch.hpp"
#include "thermocycler-gen2/errors.hpp"
SCENARIO("testing error writing") {
    GIVEN("a buffer long enough for an error") {
        auto buffer = std::string(64, 'c');
        WHEN("writing an error into it") {
            auto written =
                errors::write_into_async(buffer.begin(), buffer.end(),
                                         errors::ErrorCode::USB_TX_OVERRUN);
            THEN("the error is written into the buffer") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::StartsWith(
                                 "async ERR001:tx buffer overrun OK\n"));
                AND_THEN("the length was appropriately returned") {
                    REQUIRE(written ==
                            buffer.begin() + strlen("async ERR001:tx "
                                                    "buffer overrun OK\n"));
                }
            }
        }
    }
    GIVEN("a buffer too small for an error") {
        auto buffer = std::string(2, 'c');
        WHEN("trying to write an error into it") {
            auto written = errors::write_into_async(
                buffer.begin(), buffer.end(),
                errors::ErrorCode::INTERNAL_QUEUE_FULL);
            THEN(
                "the error written into only the space available in the "
                "buffer") {
                REQUIRE_THAT(buffer,
                             Catch::Matchers::Equals(std::string("as")));
                AND_THEN("the amount written is 0") {
                    REQUIRE(written == buffer.begin() + 2);
                }
            }
        }
    }
}