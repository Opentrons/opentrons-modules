#include "catch2/catch.hpp"
#include "thermocycler-refresh/motor_utils.hpp"

using namespace motor_util;

SCENARIO("Lid stepper current conversion works") {
    GIVEN("a target current 330 mA") {
        double current = 330.0;
        WHEN("converting") {
            auto result = LidStepper::current_to_dac(current);
            THEN("result should be the max DAC value") {
                REQUIRE(result == 0xFF);
            }
        }
    }
    GIVEN("a target current 0 mA") {
        double current = 0.0;
        WHEN("converting") {
            auto result = LidStepper::current_to_dac(current);
            THEN("result should be 0") { REQUIRE(result == 0); }
        }
    }
    GIVEN("a target current 500 mA") {
        double current = 550.0;
        WHEN("converting") {
            auto result = LidStepper::current_to_dac(current);
            THEN("result should be clamped down to max DAC value") {
                REQUIRE(result == 0xFF);
            }
        }
    }
    GIVEN("a target current 165 mA") {
        double current = 330.0 / 2.0;
        WHEN("converting") {
            auto result = LidStepper::current_to_dac(current);
            THEN("result should be half of the max DAC value") {
                REQUIRE(result == 0xFF / 2);
            }
        }
    }
}

SCENARIO("Lid stepper microstep conversion works") {
    GIVEN("a target of 1 degree") {
        double angle = 1.0F;
        WHEN("converting") {
            auto result = LidStepper::angle_to_microsteps(angle);
            THEN("the result is 1768 steps") { REQUIRE(result == 1768); }
        }
    }
    GIVEN("a target of -1 degree") {
        double angle = -1.0F;
        WHEN("converting") {
            auto result = LidStepper::angle_to_microsteps(angle);
            THEN("the result is -1768 steps") { REQUIRE(result == -1768); }
        }
    }
    GIVEN("a target of 360 degrees") {
        double angle = 360.0F;
        WHEN("converting") {
            auto result = LidStepper::angle_to_microsteps(angle);
            THEN("the result is 636800 steps") { REQUIRE(result == 636800); }
        }
    }
    GIVEN("a target of 0 degrees") {
        double angle = 0.0F;
        WHEN("converting") {
            auto result = LidStepper::angle_to_microsteps(angle);
            THEN("the result is 0 steps") { REQUIRE(result == 0); }
        }
    }
}
