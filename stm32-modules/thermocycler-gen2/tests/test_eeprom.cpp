#include "catch2/catch.hpp"
#include "test/test_at24c0xc_policy.hpp"
#include "thermocycler-gen2/eeprom.hpp"

using namespace at24c0xc_test_policy;
using namespace eeprom;

TEST_CASE("eeprom class initialization tracking") {
    GIVEN("an EEPROM") {
        auto policy = TestAT24C0XCPolicy<32>();
        auto eeprom = Eeprom<32, 0x10>();
        THEN("it starts as noninitialized") { REQUIRE(!eeprom.initialized()); }
        WHEN("reading from the EEPROM") {
            auto defaults = OffsetConstants{.a = 0, .b = 0, .c = 0};
            static_cast<void>(eeprom.get_offset_constants(defaults, policy));
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
            auto defaults = OffsetConstants{.a = 68, .b = -5, .c = 50};
            auto readback = eeprom.get_offset_constants(defaults, policy);
            THEN("the resulting constants are the defaults") {
                REQUIRE_THAT(readback.a,
                             Catch::Matchers::WithinAbs(defaults.a, 0.01));
                REQUIRE_THAT(readback.b,
                             Catch::Matchers::WithinAbs(defaults.b, 0.01));
                REQUIRE_THAT(readback.c,
                             Catch::Matchers::WithinAbs(defaults.c, 0.01));
            }
        }
    }
}

TEST_CASE("eeprom reading and writing") {
    GIVEN("an EEPROM and constants A= -3.5, B = 10 and C = -12") {
        auto policy = TestAT24C0XCPolicy<32>();
        auto eeprom = Eeprom<32, 0x10>();
        OffsetConstants constants = {.a = -3.5F, .b = 10.0F, .c = -12.0F};
        WHEN("writing the constants") {
            REQUIRE(eeprom.write_offset_constants(constants, policy));
            AND_THEN("reading back the constants") {
                auto defaults =
                    OffsetConstants{.a = -100, .b = -100, .c = -100};
                auto readback = eeprom.get_offset_constants(defaults, policy);
                THEN("the constants match") {
                    REQUIRE_THAT(readback.a,
                                 Catch::Matchers::WithinAbs(constants.a, 0.01));
                    REQUIRE_THAT(readback.b,
                                 Catch::Matchers::WithinAbs(constants.b, 0.01));
                    REQUIRE_THAT(readback.c,
                                 Catch::Matchers::WithinAbs(constants.c, 0.01));
                }
            }
        }
    }
}
