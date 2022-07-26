#include "catch2/catch.hpp"
#include "heater-shaker/flash.hpp"
#include "heater-shaker/heater_task.hpp"
#include "test/task_builder.hpp"

using namespace flash;

TEST_CASE("flash class initialization tracking") {
    GIVEN("a FLASH") {
        auto tasks = TaskBuilder::build();
        auto flash = Flash();
        THEN("it starts as noninitialized") { REQUIRE(!flash.initialized()); }
        WHEN("reading from the FLASH") {
            static_cast<void>(
                flash.get_offset_constants(tasks->get_heater_policy()));
            THEN("the FLASH now shows as initialized") {
                REQUIRE(flash.initialized());
            }
        }
    }
}

TEST_CASE("blank flash reading") {
    GIVEN("a FLASH") {
        auto tasks = TaskBuilder::build();
        auto flash = Flash();
        WHEN("reading before writing anything") {
            auto readback =
                flash.get_offset_constants(tasks->get_heater_policy());
            THEN("the resulting constants are defaults") {
                REQUIRE_THAT(readback.b,
                             Catch::Matchers::WithinAbs(-0.021, 0.01));
                REQUIRE_THAT(readback.c,
                             Catch::Matchers::WithinAbs(0.497, 0.01));
            }
        }
    }
}

TEST_CASE("flash reading and writing") {
    GIVEN("a FLASH and constants B = 10 and C = -12") {
        auto tasks = TaskBuilder::build();
        auto flash = Flash();
        OffsetConstants constants = {.b = 10.0F, .c = -12.0F, .flag = 0x01};
        WHEN("writing the constants") {
            REQUIRE(flash.set_offset_constants(constants,
                                               tasks->get_heater_policy()));
            AND_THEN("reading back the constants") {
                auto readback =
                    flash.get_offset_constants(tasks->get_heater_policy());
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
