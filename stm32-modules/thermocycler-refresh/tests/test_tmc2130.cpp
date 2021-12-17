#include <array>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/test_tmc2130_policy.hpp"
#include "thermocycler-refresh/tmc2130.hpp"

SCENARIO("tmc2130 register API works") {
    GIVEN("a tmc2130 and a blank TMC2130 register map") {
        tmc2130::TMC2130 tmc;
        TestTMC2130Policy policy;

        WHEN("setting gconfig register to all 0") {
            tmc2130::GConfig gconf;
            auto ret = tmc.set_gconf(gconf, policy);
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
            auto ret = tmc.set_gconf(gconf, policy);
            REQUIRE(ret);
            THEN("the register reads correctly") {
                auto reg_val = policy.read_register(tmc2130::Registers::GCONF);
                REQUIRE(reg_val.has_value());
                static constexpr uint32_t expected = 0x10021;
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_gconf();
                REQUIRE(readback.i_scale_analog == 1);
                REQUIRE(readback.diag0_error == 1);
                REQUIRE(readback.direct_mode == 1);
                REQUIRE(readback.stop_enable == 0);
                REQUIRE(readback.shaft == 0);
                AND_WHEN("changing a value in the register") {
                    readback.en_pwm_mode = 1;
                    REQUIRE(tmc.set_gconf(readback, policy));
                    THEN("the updated value should be reflected") {
                        gconf = tmc.get_gconf();
                        REQUIRE(gconf.en_pwm_mode == 1);
                        REQUIRE(readback.i_scale_analog == 1);
                        REQUIRE(readback.diag0_error == 1);
                        REQUIRE(readback.direct_mode == 1);
                        REQUIRE(readback.stop_enable == 0);
                        REQUIRE(readback.shaft == 0);
                    }
                }
            }
        }
        WHEN("setting CurrentControl") {
            tmc2130::CurrentControl reg{.hold_current = 0xE,
                                        .run_current = 0x1E,
                                        .hold_current_delay = 0xF};
            static constexpr uint32_t expected = 0xF1E0E;
            auto ret = tmc.set_current_control(reg, policy);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::IHOLD_IRUN);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_current_control();
                THEN("the register should be the same") {
                    REQUIRE(readback.hold_current == reg.hold_current);
                    REQUIRE(readback.run_current == reg.run_current);
                    REQUIRE(readback.hold_current_delay ==
                            reg.hold_current_delay);
                }
            }
        }
        WHEN("setting CurrentControl with invalid parameters") {
            tmc2130::CurrentControl reg{.hold_current = 0xE,
                                        .bit_padding_1 = 1,
                                        .run_current = 0x1E,
                                        .hold_current_delay = 0xF};
            auto ret = tmc.set_current_control(reg, policy);
            THEN("an error is returned") { REQUIRE(!ret); }
        }
        WHEN("setting PowerDownDelay to max time / 2") {
            auto settime = tmc2130::PowerDownDelay::max_time / 2.0F;
            auto expected_reg = tmc2130::PowerDownDelay::max_val / 2;
            REQUIRE(tmc.set_power_down_delay(settime, policy));
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::TPOWERDOWN);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected_reg);
            }
        }
        WHEN("setting a PowerDownDelay structure time") {
            tmc2130::PowerDownDelay reg;
            REQUIRE(reg.set_time(1.0F));
            THEN("the time should be updated correctly") {
                REQUIRE_THAT(reg.seconds(),
                             Catch::Matchers::WithinAbs(1.0F, 0.1));
            }
        }
        WHEN("setting TCoolThreshold") {
            tmc2130::TCoolThreshold reg{.threshold = 0xABCDE};
            static constexpr uint32_t expected = 0xABCDE;
            auto ret = tmc.set_cool_threshold(reg, policy);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::TCOOLTHRS);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_cool_threshold();
                THEN("the register should be updated") {
                    REQUIRE(readback.threshold == 0xABCDE);
                }
            }
        }
        WHEN("setting THigh") {
            tmc2130::THigh reg{.threshold = 0xABCDE};
            static constexpr uint32_t expected = 0xABCDE;
            auto ret = tmc.set_thigh(reg, policy);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val = policy.read_register(tmc2130::Registers::THIGH);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_thigh();
                THEN("the register should be updated") {
                    REQUIRE(readback.threshold == 0xABCDE);
                }
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
            static constexpr uint32_t expected = 0x2AA4E84A;
            auto ret = tmc.set_chop_config(chop, policy);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::CHOPCONF);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_chop_config();
                THEN("the register should be updated") {
                    REQUIRE(readback.toff == chop.toff);
                    REQUIRE(readback.hstrt == chop.hstrt);
                    REQUIRE(readback.fd3 == chop.fd3);
                    REQUIRE(readback.disfdcc == chop.disfdcc);
                    REQUIRE(readback.chm == chop.chm);
                    REQUIRE(readback.sync == chop.sync);
                }
            }
        }
        WHEN("setting CoolConfig") {
            tmc2130::CoolConfig cool{.semin = 0,
                                     .seup = 1,
                                     .semax = 3,
                                     .sedn = 1,
                                     .seimin = 0,
                                     .sgt = 64,
                                     .sfilt = 0};
            static constexpr uint32_t expected = 0x402320;
            auto ret = tmc.set_cool_config(cool, policy);
            REQUIRE(ret);
            THEN("the register should be set correctly") {
                auto reg_val =
                    policy.read_register(tmc2130::Registers::COOLCONF);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_cool_config();
                THEN("the register should be updated") {
                    REQUIRE(readback.semin == cool.semin);
                    REQUIRE(readback.seup == cool.seup);
                    REQUIRE(readback.semax == cool.semax);
                    REQUIRE(readback.sedn == cool.sedn);
                    REQUIRE(readback.seimin == cool.seimin);
                    REQUIRE(readback.sgt == cool.sgt);
                    REQUIRE(readback.sfilt == cool.sfilt);
                }
            }
            AND_WHEN("trying to set invalid register") {
                cool.padding_1 = 1;
                ret = tmc.set_cool_config(cool, policy);
                THEN("the writing fails and the register is left alone") {
                    REQUIRE(!ret);
                    REQUIRE(tmc.get_cool_config().padding_1 == 0);
                }
            }
        }
    }
}
