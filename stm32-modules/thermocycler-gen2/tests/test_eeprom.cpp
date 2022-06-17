#include "catch2/catch.hpp"
#include "test/test_at24c0xc_policy.hpp"
#include "thermocycler-gen2/eeprom.hpp"

using namespace at24c0xc_test_policy;
using namespace eeprom;

static auto _default = OffsetConstants{
    .a = 68, .bl = -5, .cl = -4, .bc = -1.5, .cc = 0, .br = 2, .cr = 50.2};

TEST_CASE("eeprom class initialization tracking") {
    GIVEN("an EEPROM") {
        auto policy = TestAT24C0XCPolicy<32>();
        auto eeprom = Eeprom<32, 0x10>();
        THEN("it starts as noninitialized") { REQUIRE(!eeprom.initialized()); }
        WHEN("reading from the EEPROM") {
            auto defaults = _default;
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
            auto defaults = _default;
            auto readback = eeprom.get_offset_constants(defaults, policy);
            THEN("the resulting constants are the defaults") {
                REQUIRE_THAT(readback.a,
                             Catch::Matchers::WithinAbs(defaults.a, 0.01));
                REQUIRE_THAT(readback.bl,
                             Catch::Matchers::WithinAbs(defaults.bl, 0.01));
                REQUIRE_THAT(readback.cl,
                             Catch::Matchers::WithinAbs(defaults.cl, 0.01));
                REQUIRE_THAT(readback.bc,
                             Catch::Matchers::WithinAbs(defaults.bc, 0.01));
                REQUIRE_THAT(readback.cc,
                             Catch::Matchers::WithinAbs(defaults.cc, 0.01));
                REQUIRE_THAT(readback.br,
                             Catch::Matchers::WithinAbs(defaults.br, 0.01));
                REQUIRE_THAT(readback.cr,
                             Catch::Matchers::WithinAbs(defaults.cr, 0.01));
            }
        }
    }
}

TEST_CASE("eeprom reading and writing") {
    GIVEN("an EEPROM and constants A= -3.5, B = 10 and C = -12") {
        auto policy = TestAT24C0XCPolicy<32>();
        auto eeprom = Eeprom<32, 0x10>();
        OffsetConstants constants = {.a = 32,
                                     .bl = -33,
                                     .cl = -44,
                                     .bc = -1.55,
                                     .cc = 0.51,
                                     .br = 1,
                                     .cr = 99.99};
        WHEN("writing the constants") {
            REQUIRE(eeprom.write_offset_constants(constants, policy));
            AND_THEN("reading back the constants") {
                auto defaults = _default;
                auto readback = eeprom.get_offset_constants(defaults, policy);
                THEN("the constants match") {
                    REQUIRE_THAT(readback.a,
                                 Catch::Matchers::WithinAbs(constants.a, 0.01));
                    REQUIRE_THAT(readback.bl, Catch::Matchers::WithinAbs(
                                                  constants.bl, 0.01));
                    REQUIRE_THAT(readback.cl, Catch::Matchers::WithinAbs(
                                                  constants.cl, 0.01));
                    REQUIRE_THAT(readback.bc, Catch::Matchers::WithinAbs(
                                                  constants.bc, 0.01));
                    REQUIRE_THAT(readback.cc, Catch::Matchers::WithinAbs(
                                                  constants.cc, 0.01));
                    REQUIRE_THAT(readback.br, Catch::Matchers::WithinAbs(
                                                  constants.br, 0.01));
                    REQUIRE_THAT(readback.cr, Catch::Matchers::WithinAbs(
                                                  constants.cr, 0.01));
                }
            }
        }
    }
}
