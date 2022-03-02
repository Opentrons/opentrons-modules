#include "catch2/catch.hpp"
#include "test/test_at24c0xc_policy.hpp"
#include "thermocycler-refresh/eeprom.hpp"

using namespace at24c0xc_test_policy;
using namespace eeprom;

TEST_CASE("eeprom class initialization tracking") {
    GIVEN("an EEPROM") {
        auto policy = TestAT24C0XCPolicy<32>();
        auto eeprom = Eeprom<32, 0x10>();
        THEN("it starts as noninitialized") { REQUIRE(!eeprom.initialized()); }
        WHEN("reading from the EEPROM") {
            static_cast<void>(eeprom.get_offset_constants(policy));
            THEN("the EEPROM now shows as initialized") {
                REQUIRE(eeprom.initialized());
            }
        }
    }
}

TEST_CASE("blank eeprom reading") {
    GIVEN("an EEPROM") {
        auto policy = TestAT24C0XCPolicy<32>();
        auto eeprom = Eeprom<32, 0x10>();
        WHEN("reading before writing anything") {
            auto readback = eeprom.get_offset_constants(policy);
            THEN("the resulting constants are 0") {
                REQUIRE_THAT(readback.b, Catch::Matchers::WithinAbs(0, 0.01));
                REQUIRE_THAT(readback.c, Catch::Matchers::WithinAbs(0, 0.01));
            }
        }
    }
}

TEST_CASE("eeprom reading and writing") {
    GIVEN("an EEPROM and constants B = 10 and C = -12") {
        auto policy = TestAT24C0XCPolicy<32>();
        auto eeprom = Eeprom<32, 0x10>();
        OffsetConstants constants = {.b = 10.0F, .c = -12.0F};
        WHEN("writing the constants") {
            REQUIRE(eeprom.write_offset_constants(constants, policy));
            AND_THEN("reading back the constants") {
                auto readback = eeprom.get_offset_constants(policy);
                THEN("the constants match") {
                    REQUIRE_THAT(readback.b,
                                 Catch::Matchers::WithinAbs(constants.b, 0.01));
                    REQUIRE_THAT(readback.c,
                                 Catch::Matchers::WithinAbs(constants.c, 0.01));
                }
            }
        }
    }
}
