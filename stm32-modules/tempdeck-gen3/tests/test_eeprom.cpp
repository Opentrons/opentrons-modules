#include "catch2/catch.hpp"
#include "tempdeck-gen3/eeprom.hpp"
#include "test/test_m24128_policy.hpp"

using namespace m24128_test_policy;
using namespace eeprom;

static auto _default = OffsetConstants{.a = 68, .b = 5, .c = 9};

TEST_CASE("eeprom class initialization tracking") {
    GIVEN("an EEPROM") {
        auto policy = TestM24128Policy();
        auto eeprom = Eeprom<0x10>();
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
        auto policy = TestM24128Policy();
        auto eeprom = Eeprom<0x10>();
        WHEN("reading before writing anything") {
            auto defaults = _default;
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
        auto policy = TestM24128Policy();
        auto eeprom = Eeprom<0x10>();
        OffsetConstants constants = {.a = 32, .b = -33, .c = -44};
        WHEN("writing the constants") {
            REQUIRE(eeprom.write_offset_constants(constants, policy));
            AND_THEN("reading back the constants") {
                auto defaults = _default;
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
