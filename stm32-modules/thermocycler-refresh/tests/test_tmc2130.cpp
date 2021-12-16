#include <array>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "thermocycler-refresh/tmc2130.hpp"
#include "test/test_tmc2130_policy.hpp"

// Full register map to make iterating less painful
static constexpr std::array<tmc2130::Registers, 11> _registers = {
    tmc2130::Registers::general_config,
    tmc2130::Registers::general_status,
    tmc2130::Registers::io_input,
    tmc2130::Registers::ihold_irun,
    tmc2130::Registers::tpower_down,
    tmc2130::Registers::tstep,
    tmc2130::Registers::tpwmthrs,
    tmc2130::Registers::tcoolthrs,
    tmc2130::Registers::thigh,
    tmc2130::Registers::xdirect,
    tmc2130::Registers::vdcmin,
};

SCENARIO("tmc2130 register logic works") {
    GIVEN("a tmc2130 and a blank register map") {
        tmc2130::TMC2130 tmc;
        TestTMC2130Policy policy;

        WHEN("setting gconfig register to all 0") {
            tmc2130::GConf gconf;
            auto ret = tmc.set_register(policy, gconf);
            REQUIRE(ret);
            THEN("the register reads all 0's") {
                auto reg_val = policy.read_register(tmc2130::Registers::general_config);
                REQUIRE(reg_val.has_value());
                REQUIRE(reg_val.value() == 0);
            }
        }
        WHEN("setting gconfig register to have some nonzero elements") {
            tmc2130::GConf gconf {
                .i_scale_analog = 1,
                .diag0_error = 1,
                .direct_mode = 1
            };
            auto ret = tmc.set_register(policy, gconf);
            REQUIRE(ret);
            THEN("the register reads correctly") {
                auto reg_val = policy.read_register(tmc2130::Registers::general_config);
                REQUIRE(reg_val.has_value());
                static constexpr uint64_t expected = 0x10021;
                REQUIRE(reg_val.value() == expected);
            }
            AND_WHEN("reading back the register") {
                auto readback = tmc.get_register<tmc2130::GConf>(policy);
                REQUIRE(readback.has_value());
                auto readback_val = readback.value();
                REQUIRE(readback_val.i_scale_analog == 1);
                REQUIRE(readback_val.diag0_error == 1);
                REQUIRE(readback_val.direct_mode == 1);
                REQUIRE(readback_val.stop_enable == 0);
                REQUIRE(readback_val.shaft == 0);
            }
        }
    }
}
