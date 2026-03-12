#include <catch2/catch.hpp>

#include "firmware/pressure_controller.hpp"
#include "systemwide.h"

namespace pressure_controller {

TEST_CASE("PressureController - Basic Configuration", "[controller]") {
    PressureController ctrl;

    SECTION("Default state after construction") {
        auto state = ctrl.get_state();
        REQUIRE(state.kp == Approx(KP));
        REQUIRE(state.ki == Approx(KI));
        REQUIRE(state.kd == Approx(KD));
        REQUIRE(state.k_velocity == Approx(20.0));
        REQUIRE(state.k_holding == Approx(43.0));
        REQUIRE(state.overshoot == Approx(-2.0));
        REQUIRE(state.ramp_rate == Approx(DEFAULT_RAMP_RATE));
    }

    SECTION("configure_slew sets ramp rate and initial value") {
        ctrl.configure_slew(1013.0, DEFAULT_RAMP_RATE);
        auto state = ctrl.get_state();
        REQUIRE(state.ramp_rate == Approx(DEFAULT_RAMP_RATE));
    }

    SECTION("configure_pid updates tunings") {
        ctrl.configure_pid(10.0, 5.0, 0.2, 25.0, 50.0, -3.0, true);
        auto state = ctrl.get_state();
        REQUIRE(state.kp == Approx(10.0));
        REQUIRE(state.ki == Approx(5.0));
        REQUIRE(state.kd == Approx(0.2));
        REQUIRE(state.k_velocity == Approx(25.0));
        REQUIRE(state.k_holding == Approx(50.0));
        REQUIRE(state.overshoot == Approx(-3.0));
    }
}

TEST_CASE("PressureController - Update Logic", "[controller]") {
    PressureController ctrl;
    ctrl.configure_slew(1013.0, DEFAULT_RAMP_RATE);

    SECTION("Overshoot disables feed-forward") {
        double rpm = ctrl.update(0.04, 1013.0, 1010.0);  // very small error
        // Should be close to pure PID output (no FF)
        REQUIRE(rpm > 0.0);
    }

    SECTION("Feed-forward adds during normal ramp") {
        // Simulate a ramp down
        ctrl.reset();
        double rpm = ctrl.update(0.04, 1000.0, 500.0);
        rpm = ctrl.update(0.05, 990.0, 500.0);
        rpm = ctrl.update(0.06, 980.0, 500.0);
        REQUIRE(rpm > 0.0);  // FF should contribute
    }

    SECTION("RPM is clamped") {
        // high gain == huge positive error
        ctrl.configure_pid(100.0, 0.0, 0.0, 0.0, 0.0, -100.0);
        double rpm = ctrl.update(0.04, 0.0, 1013.0);
        REQUIRE(rpm <= MAX_RPM);
    }

    SECTION("Slew limiter smooths target") {
        // slow ramp
        ctrl.configure_slew(1013.0, 50.0);
        double first_rpm = ctrl.update(0.04, 1013.0, 200.0);
        double second_rpm = ctrl.update(0.04, 1013.0, 200.0);
        REQUIRE(second_rpm > first_rpm);  // ramp is progressing
    }
}

TEST_CASE("PressureController - Reset", "[controller]") {
    PressureController ctrl;
    ctrl.configure_slew(1013.0, DEFAULT_RAMP_RATE);

    ctrl.update(0.04, 900.0, 800.0);

    ctrl.reset();

    auto state = ctrl.get_state();
    REQUIRE(state.ramp_rate == Approx(DEFAULT_RAMP_RATE));
}

TEST_CASE("PressureController - Full Ramp Example", "[controller][example]") {
    PressureController ctrl;
    ctrl.configure_slew(1013.0, DEFAULT_RAMP_RATE);

    double current = 1013.0;
    double target = 200.0;

    for (int i = 0; i < 50; ++i) {  // ~2 seconds
        double rpm = ctrl.update(0.04, current, target);
        current = current - (rpm * 0.5);  // ramping down
    }

    REQUIRE(current < 600.0);  // should have made progress
}

}  // namespace pressure_controller
