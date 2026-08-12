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

    SECTION("configure_bands updates and clamps") {
        ctrl.configure_bands(100.0, SLEW_END_FRACTION);
        auto state = ctrl.get_state();
        REQUIRE(state.approach_band == Approx(100.0));
        REQUIRE(state.slew_end_fraction == Approx(SLEW_END_FRACTION));
        ctrl.configure_bands(0.0, 1.5);
        state = ctrl.get_state();
        REQUIRE(state.approach_band == Approx(MIN_APPROACH_BAND_MBAR));
        REQUIRE(state.slew_end_fraction == Approx(MAX_SLEW_END_FRACTION));
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

    SECTION("Overshoot threshold scales with vacuum depth") {
        PressureController ctrl;
        auto deep = ctrl.compute_effective_overshoot(200.0);
        auto shallow = ctrl.compute_effective_overshoot(900.0);
        REQUIRE(std::abs(deep) > std::abs(shallow));
        REQUIRE(deep == Approx(-16.26).margin(0.1));
        REQUIRE(shallow == Approx(-2.27).margin(0.1));
    }

    SECTION("Pressure slew slows in the final vacuum depth") {
        ctrl.configure_slew(900.0, 100.0);
        ctrl.update(0.04, 900.0, 200.0);
        auto far_step = 900.0 - ctrl.get_smooth_target();

        ctrl.reset();
        ctrl.configure_slew(250.0, 100.0);
        ctrl.update(0.04, 250.0, 200.0);
        auto near_step = 250.0 - ctrl.get_smooth_target();

        REQUIRE(near_step < far_step);
    }

    SECTION("Feed-forward tapers near the slewed trajectory") {
        ctrl.configure_slew(1013.0, DEFAULT_RAMP_RATE);
        auto rpm_far = ctrl.update(0.04, 1013.0, 200.0);
        ctrl.reset();
        ctrl.configure_slew(250.0, DEFAULT_RAMP_RATE);
        auto rpm_near = ctrl.update(0.04, 250.0, 200.0);
        REQUIRE(rpm_near < rpm_far);
    }

    SECTION("Overshoot past final target commands zero RPM") {
        ctrl.configure_slew(200.0, DEFAULT_RAMP_RATE);
        auto rpm = ctrl.update(0.04, 180.0, 200.0);
        REQUIRE(rpm == Approx(0.0));
    }

    SECTION("Overshoot clears accumulated integral term") {
        ctrl.configure_pid(0.0, 10.0, 0.0, 0.0, 0.0, -2.0, true);
        ctrl.configure_slew(500.0, DEFAULT_RAMP_RATE);
        for (int i = 0; i < 30; ++i) {
            ctrl.update(0.04, 500.0, 200.0);
        }
        auto rpm_during_overshoot = ctrl.update(0.04, 420.0, 200.0);
        REQUIRE(rpm_during_overshoot == Approx(0.0));
    }

    SECTION("Small residual short of final target still builds integral") {
        // Pure I, no FF: a few mbar short of final must not zero the
        // integrator.
        ctrl.configure_pid(0.0, 10.0, 0.0, 0.0, 0.0, -2.0, true);
        // Trajectory already at final target (no slew motion).
        ctrl.configure_slew(610.0, DEFAULT_RAMP_RATE);
        double rpm_early = 0.0;
        double rpm_late = 0.0;
        for (int i = 0; i < 20; ++i) {
            // Final target 600 abs; current 608 => ~8 mbar shallow residual.
            rpm_late = ctrl.update(0.04, 608.0, 600.0);
            if (i == 0) {
                rpm_early = rpm_late;
            }
        }
        REQUIRE(rpm_late > rpm_early);
        REQUIRE(rpm_late > 0.0);
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
        current -= rpm * 0.02;  // simplified plant model
    }

    REQUIRE(current < 950.0);  // should have made progress
}

}  // namespace pressure_controller
