#include "catch2/catch.hpp"

#include "core/at24c0xc.hpp"
#include "test/test_at24c0xc_policy.hpp"

TEST_CASE("TestAT24C0XCPolicy functionality") {
    GIVEN("a test policy of 32 pages") {
        auto policy = TestAT24C0XCPolicy<32>();
        WHEN("writing a full page") {
            std::array<uint8_t, 9> buffer = {
                0, 0, 1, 2, 3, 4, 5, 6, 7 };
            REQUIRE(policy.i2c_write(0, buffer));
            THEN("the internal buffer matches what was written") {
                for(int i = 0; i < 8; ++i) {
                    DYNAMIC_SECTION(std::string("Buffer index ") << i) {
                        REQUIRE(policy._buffer[i] == buffer[i+1]);
                    }
                }
            }
        }
    }
}
