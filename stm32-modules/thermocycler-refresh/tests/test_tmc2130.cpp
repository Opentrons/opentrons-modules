#include <array>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/test_tmc2130_policy.hpp"
#include "thermocycler-refresh/tmc2130.hpp"

SCENARIO("tmc2130 register structures are defined correctly") {
    REQUIRE(sizeof(tmc2130::GConfig) <= sizeof(uint32_t));
    REQUIRE(sizeof(tmc2130::GStatus) <= sizeof(uint32_t));
    REQUIRE(sizeof(tmc2130::CurrentControl) <= sizeof(uint32_t));
    REQUIRE(sizeof(tmc2130::PowerDownDelay) <= sizeof(uint32_t));
    REQUIRE(sizeof(tmc2130::TCoolThreshold) <= sizeof(uint32_t));
    REQUIRE(sizeof(tmc2130::THigh) <= sizeof(uint32_t));
    REQUIRE(sizeof(tmc2130::ChopConfig) <= sizeof(uint32_t));
    REQUIRE(sizeof(tmc2130::CoolConfig) <= sizeof(uint32_t));
}

SCENARIO("tmc2130 interface class API works") {
    GIVEN("a tmc2130 interface and a blank register map") {
        tmc2130::TMC2130Interface spi;
        TestTMC2130Policy policy;
        REQUIRE(policy.read_register(tmc2130::Registers::GCONF).has_value());
        WHEN("constructing a read message") {
            auto ret = spi.build_message(tmc2130::Registers::GCONF,
                                         tmc2130::WriteFlag::READ, 0);
            REQUIRE(ret.has_value());
            THEN("the message is constructed correctly") {
                auto buf = ret.value();
                REQUIRE((buf[0] & 0x80) == 0x00);
                REQUIRE((buf[0] & ~0x80) ==
                        static_cast<uint8_t>(tmc2130::Registers::GCONF));
            }
        }
        WHEN("constructing a write message") {
            auto ret = spi.build_message(tmc2130::Registers::GCONF,
                                         tmc2130::WriteFlag::WRITE, 0xABCDEF01);
            REQUIRE(ret.has_value());
            THEN("the message is constructed correctly") {
                auto buf = ret.value();
                REQUIRE((buf[0] & 0x80) == 0x80);
                REQUIRE((buf[0] & ~0x80) ==
                        static_cast<uint8_t>(tmc2130::Registers::GCONF));
                REQUIRE(buf[1] == 0xAB);
                REQUIRE(buf[2] == 0xCD);
                REQUIRE(buf[3] == 0xEF);
                REQUIRE(buf[4] == 0x01);
            }
        }
        WHEN("Writing to a register") {
            spi.write(tmc2130::Registers::GCONF, 0xABCDEF01, policy);
            THEN("the value in the policy is updated") {
                auto ret = policy.read_register(tmc2130::Registers::GCONF);
                REQUIRE(ret.has_value());
                REQUIRE(ret.value() == 0xABCDEF01);
            }
            AND_WHEN("Reading from that register") {
                auto ret = spi.read(tmc2130::Registers::GCONF, policy);
                THEN("the value in the policy is returned") {
                    REQUIRE(ret.has_value());
                    REQUIRE(ret.value() == 0xABCDEF01);
                }
            }
        }
    }
}

SCENARIO("tmc2130 register API works") {
    GIVEN("a tmc2130 and a blank TMC2130 register map") {
        auto tmc = tmc2130::TMC2130(tmc2130::TMC2130RegisterMap());
        TestTMC2130Policy policy;

        THEN("the TMC2130 should not be initialized yet") {
            REQUIRE(!tmc.initialized());
        }

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
                auto readback = tmc.get_gconf(policy).value();
                REQUIRE(readback.i_scale_analog == 1);
                REQUIRE(readback.diag0_error == 1);
                REQUIRE(readback.direct_mode == 1);
                REQUIRE(readback.stop_enable == 0);
                REQUIRE(readback.shaft == 0);
                AND_WHEN("changing a value in the register") {
                    readback.en_pwm_mode = 1;
                    REQUIRE(tmc.set_gconf(readback, policy));
                    THEN("the updated value should be reflected") {
                        gconf = tmc.get_gconf(policy).value();
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
                auto readback = tmc.get_register_map().ihold_irun;
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
            THEN("the written value doesn't have the padding set") {
                REQUIRE(ret);
                REQUIRE(tmc.get_register_map().ihold_irun.bit_padding_1 == 0);
            }
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
            THEN("reading back the time should give the right time") {
                REQUIRE(tmc.get_register_map().tpowerdown.time == expected_reg);
            }
        }
        WHEN("setting a PowerDownDelay structure time") {
            tmc2130::PowerDownDelay reg = {
                .time = tmc2130::PowerDownDelay::seconds_to_reg(1.0F)};
            THEN("the time should be updated correctly") {
                REQUIRE_THAT(tmc2130::PowerDownDelay::reg_to_seconds(reg.time),
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
                auto readback = tmc.get_register_map().tcoolthrs;
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
                auto readback = tmc.get_register_map().thigh;
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
                auto readback = tmc.get_chop_config(policy).value();
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
                auto readback = tmc.get_register_map().coolconf;
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
            AND_WHEN("trying to set invalid padding bits") {
                cool.padding_1 = 1;
                ret = tmc.set_cool_config(cool, policy);
                THEN("the writing succeeds and the padding bits are empty") {
                    REQUIRE(ret);
                    REQUIRE(tmc.get_register_map().coolconf.padding_1 == 0);
                }
            }
        }
    }
    GIVEN("a tmc2130 with a randomly allocated register map") {
        tmc2130::TMC2130RegisterMap registers = {
            .gconfig = {.en_pwm_mode = 1},
            .ihold_irun = {.hold_current = 0x0,
                           .run_current = 0x2,
                           .hold_current_delay = 0x7},
            .tpowerdown = {},
            .tcoolthrs = {},
            .thigh = {.threshold = 0xFFFFF},
            .chopconf = {.toff = 5, .hstrt = 5, .hend = 3, .tbl = 2, .mres = 4},
            .coolconf = {.sgt = 6}};
        auto tmc = tmc2130::TMC2130(registers);
        auto policy = TestTMC2130Policy();
        WHEN("writing configuration registers") {
            tmc.write_config(policy);
            THEN("registers are modified appropriately") {
                REQUIRE(tmc.get_gconf(policy).value().en_pwm_mode == 1);
                REQUIRE(tmc.get_gconf(policy).value().diag0_int_pushpull == 0);
                REQUIRE(tmc.get_register_map().ihold_irun.hold_current == 0);
                REQUIRE(tmc.get_register_map().ihold_irun.run_current == 2);
                REQUIRE(tmc.get_register_map().ihold_irun.hold_current_delay ==
                        7);
                REQUIRE(tmc.get_register_map().tpowerdown.time == 0.0F);
                REQUIRE(tmc.get_register_map().tcoolthrs.threshold == 0);
                REQUIRE(tmc.get_register_map().thigh.threshold == 0xFFFFF);
                REQUIRE(tmc.get_chop_config(policy).value().toff == 5);
                REQUIRE(tmc.get_chop_config(policy).value().hstrt == 5);
                REQUIRE(tmc.get_chop_config(policy).value().hend == 3);
                REQUIRE(tmc.get_chop_config(policy).value().tbl == 2);
                REQUIRE(tmc.get_chop_config(policy).value().mres == 4);
                REQUIRE(tmc.get_register_map().coolconf.sgt == 6);
            }
            THEN("the TMC2130 reads as initialized") {
                REQUIRE(tmc.initialized());
            }
        }
        AND_WHEN("overwriting configuration registers") {
            tmc2130::TMC2130RegisterMap registers_2 = {
                .tpowerdown = {.time =
                                   tmc2130::PowerDownDelay::seconds_to_reg(2)}};
            tmc.write_config(registers_2, policy);
            THEN("registers are modified appropriately") {
                REQUIRE(tmc.get_gconf(policy).value().en_pwm_mode == 0);
                REQUIRE(tmc.get_gconf(policy).value().diag0_int_pushpull == 0);
                REQUIRE(tmc.get_register_map().ihold_irun.hold_current == 0);
                REQUIRE(tmc.get_register_map().ihold_irun.run_current == 0);
                REQUIRE(tmc.get_register_map().ihold_irun.hold_current_delay ==
                        0);
                REQUIRE(tmc.get_register_map().tpowerdown.time ==
                        tmc2130::PowerDownDelay::seconds_to_reg(2));
                REQUIRE(tmc.get_register_map().tcoolthrs.threshold == 0);
                REQUIRE(tmc.get_register_map().thigh.threshold == 0);
                REQUIRE(tmc.get_chop_config(policy).value().toff == 0);
                REQUIRE(tmc.get_chop_config(policy).value().hstrt == 0);
                REQUIRE(tmc.get_chop_config(policy).value().hend == 0);
                REQUIRE(tmc.get_chop_config(policy).value().mres == 0);
                REQUIRE(tmc.get_register_map().coolconf.sgt == 0);
            }
        }
    }
}
