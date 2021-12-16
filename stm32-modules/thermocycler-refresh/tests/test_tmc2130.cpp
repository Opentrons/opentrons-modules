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
    }
}
