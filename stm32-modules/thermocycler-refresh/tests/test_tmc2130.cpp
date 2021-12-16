#include <array>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/test_tmc2130_policy.hpp"
#include "thermocycler-refresh/tmc2130.hpp"

SCENARIO("tmc2130 register structures work") {
    GIVEN("a tmc2130 and a blank register map") {
        tmc2130::TMC2130 tmc;
        TestTMC2130Policy policy;

        WHEN("setting gconfig register to all 0") {
            tmc2130::GConfig gconf;
            auto ret = tmc.set_register(policy, gconf);
            REQUIRE(ret);
            THEN("the register reads all 0's") {
                auto reg_val = policy.read_register(tmc2130::Registers::GCONF);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == 0);
            }
        }
        WHEN("setting gconfig register to have some nonzero elements") {
            tmc2130::GConfig gconf{
                .i_scale_analog = 1, .diag0_error = 1, .direct_mode = 1};
            auto ret = tmc.set_register(policy, gconf);
            REQUIRE(ret);
            THEN("the register reads correctly") {
                auto reg_val = policy.read_register(tmc2130::Registers::GCONF);
                REQUIRE(reg_val.has_value());
                static constexpr uint64_t expected = 0x10021;
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_register<tmc2130::GConfig>(policy);
                REQUIRE(readback.has_value());
                auto readback_val = readback.value();
                REQUIRE(readback_val.i_scale_analog == 1);
                REQUIRE(readback_val.diag0_error == 1);
                REQUIRE(readback_val.direct_mode == 1);
                REQUIRE(readback_val.stop_enable == 0);
                REQUIRE(readback_val.shaft == 0);
            }
        }
        WHEN("setting CurrentControl") {
            tmc2130::CurrentControl reg{.hold_current = 0xE,
                                        .run_current = 0x1E,
                                        .hold_current_delay = 0xF};
            static constexpr uint64_t expected = 0xF1E0E;
            auto ret = tmc.set_register(policy, reg);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::IHOLD_IRUN);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
        }
        WHEN("setting PowerDownDelay to max time / 2") {
            tmc2130::PowerDownDelay reg{
                .time = tmc2130::PowerDownDelay::max_val / 2};
            static constexpr double expected_time =
                tmc2130::PowerDownDelay::max_time / 2;
            REQUIRE_THAT(reg.seconds(),
                         Catch::Matchers::WithinAbs(expected_time, 0.1));
            auto ret = tmc.set_register(policy, reg);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::TPOWERDOWN);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() ==
                        tmc2130::PowerDownDelay::max_val / 2);
            }
            THEN("setting the struct's time to 1 second") {
                REQUIRE(reg.set_time(1.0F));
                THEN("the time should be updated correctly") {
                    REQUIRE_THAT(reg.seconds(),
                                 Catch::Matchers::WithinAbs(1.0F, 0.1));
                }
            }
        }
        WHEN("setting TCoolThreshold") {
            tmc2130::TCoolThreshold reg{.threshold = 0xABCDE};
            static constexpr uint64_t expected = 0xABCDE;
            auto ret = tmc.set_register(policy, reg);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::TCOOLTHRS);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
        }
        WHEN("setting THigh") {
            tmc2130::THigh reg{.threshold = 0xABCDE};
            static constexpr uint64_t expected = 0xABCDE;
            auto ret = tmc.set_register(policy, reg);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val = policy.read_register(tmc2130::Registers::THIGH);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
        }
        WHEN("setting ChopConfig") {
            tmc2130::ChopConfig chop{.toff = 0xA,
                                     .hstrt = 4,
                                     .hend = 0,
                                     .fd3 = 1,
                                     .disfdcc = 0,
                                     .rndtf = 1,
                                     .chm = 1,
                                     .tbl = 1,
                                     .vsense = 0,
                                     .vhighfs = 1,
                                     .vhighchm = 0,
                                     .sync = 0xA,
                                     .mres = 0xA,
                                     .intpol = 0,
                                     .dedge = 1,
                                     .diss2g = 0};
            static constexpr uint64_t expected = 0x2AA4E84A;
            auto ret = tmc.set_register(policy, chop);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::CHOPCONF);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
        }
    }
}
