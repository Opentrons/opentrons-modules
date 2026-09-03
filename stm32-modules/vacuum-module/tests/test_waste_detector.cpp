#include <catch2/catch.hpp>

#include "firmware/waste_detector.hpp"

namespace waste_detector {

namespace {

auto samples_for(double ms) -> int {
    return static_cast<int>(ms / CONTROL_PERIOD_MS);
}

auto run_ramp(WasteDetector& detector, double atm, double target,
              double start_p, double step, uint32_t dt_ms, int ticks,
              double orifice_dp, double rpm) -> WasteFullError {
    auto err = WasteFullError::NO_ERROR;
    double p = start_p;
    uint32_t t = 0;
    for (int i = 0; i < ticks; ++i) {
        t += dt_ms;
        p += step;
        err = detector.check(t, p + orifice_dp, p, target, atm, rpm);
        if (err != WasteFullError::NO_ERROR) {
            return err;
        }
    }
    return err;
}

auto enter_hold(WasteDetector& detector, double atm, double target,
                double orifice_dp, double rpm) -> void {
    double p = target + 3.0;
    uint32_t t = 20000;
    const uint32_t samples = (NEAR_TARGET_MS / CONTROL_PERIOD_MS) + 5;
    for (uint32_t i = 0; i < samples; ++i) {
        t += 40;
        detector.check(t, p + orifice_dp, p, target, atm, rpm);
    }
}

auto hold_for(WasteDetector& detector, double atm, double target, int ticks,
              double orifice_dp, double rpm, double offset = 3.0)
    -> WasteFullError {
    auto err = WasteFullError::NO_ERROR;
    const double p = target + offset;
    uint32_t t = 40000;
    for (int i = 0; i < ticks; ++i) {
        t += 40;
        err = detector.check(t, p + orifice_dp, p, target, atm, rpm);
        if (err != WasteFullError::NO_ERROR) {
            return err;
        }
    }
    return err;
}

}  // namespace

TEST_CASE("WasteDetector - Core Behavior", "[waste][detector]") {
    WasteDetector detector;

    SECTION("Default state after construction") {
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
        REQUIRE(detector.check(0, 1013.0, 1013.0, 200.0, 1013.0) ==
                WasteFullError::NO_ERROR);
    }

    SECTION("Zero vacuum depth is ignored") {
        auto err = WasteFullError::NO_ERROR;
        for (int i = 0; i < 80; ++i) {
            err = detector.check(static_cast<uint32_t>(i * 40), 1011.0, 1010.0,
                                 1013.0, 1013.0, 0.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Disabled detector never trips") {
        auto cfg = detector.get_config();
        cfg.enable_waste_full = false;
        detector.configure(cfg);
        auto err =
            run_ramp(detector, 1013.0, 500.0, 1010.0, -40.0, 10, 40, 2.0, 0.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION("reset() clears a hold trip") {
        const double target = 500.0;
        run_ramp(detector, 1013.0, target, 1010.0, -5.0, 80, 120, 2.0, 0.0);
        enter_hold(detector, 1013.0, target, 2.0, 20.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 2.0, 20.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
        detector.reset();
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
        REQUIRE(detector.check(0, 1013.0, 1013.0, target, 1013.0) ==
                WasteFullError::NO_ERROR);
    }

    SECTION("Hold hunting toward atmosphere is not full") {
        run_ramp(detector, 1013.0, 813.0, 1010.0, -3.0, 80, 120, 12.0, 400.0);
        enter_hold(detector, 1013.0, 813.0, 3.0, 250.0);

        double p = 813.0;
        uint32_t t = 50000;
        for (int i = 0; i < 30; ++i) {
            t += 40;
            p += (i % 2) ? 3.0 : -2.0;
            auto err = detector.check(t, p + 3.0, p, 813.0, 1013.0, 250.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Shallow leaky hold is not full") {
        run_ramp(detector, 1013.0, 813.0, 1010.0, -3.0, 80, 120, 2.0, 250.0);
        enter_hold(detector, 1013.0, 813.0, 2.0, 250.0);
        auto err = hold_for(detector, 1013.0, 813.0,
                            samples_for(STABLE_HOLD_MS) + 20, 2.0, 250.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION("Shallow overshoot deadhead is full") {
        const double target = 813.0;  // 200 mbar
        run_ramp(detector, 1013.0, target, 1010.0, -3.0, 80, 120, 2.0, 0.0);
        enter_hold(detector, 1013.0, target, 2.0, 0.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 2.0, 0.0, -15.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Sealed credit waits for near-target debounce") {
        const double target = 500.0;
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS), 2.0, 20.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION("Mid-depth sealed deadhead is full") {
        const double target = 500.0;
        run_ramp(detector, 1013.0, target, 1010.0, -5.0, 80, 120, 2.0, 0.0);
        enter_hold(detector, 1013.0, target, 2.0, 20.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 2.0, 20.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Mid-depth sealed hold survives periodic RPM blips") {
        const double atm = 1013.0;
        const double target = 500.0;
        run_ramp(detector, atm, target, 1010.0, -5.0, 80, 120, 2.0, 20.0);
        enter_hold(detector, atm, target, 2.0, 20.0);

        auto err = WasteFullError::NO_ERROR;
        uint32_t t = 40000;
        const double p = target + 3.0;
        for (int i = 0; i < samples_for(STABLE_HOLD_MS) + 60; ++i) {
            t += 40;
            const double rpm = ((i % 10) == 0) ? 400.0 : 20.0;
            err = detector.check(t, p + 2.0, p, target, atm, rpm);
            if (err == WasteFullError::FLOW_STABLE_FULL_ERROR) {
                break;
            }
        }
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Mid-depth leak RPM is not full") {
        const double target = 313.0;  // 700 mbar depth
        run_ramp(detector, 1013.0, target, 1010.0, -8.0, 80, 140, 2.0, 800.0);
        enter_hold(detector, 1013.0, target, 2.0, 800.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 2.0, 800.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION("Hold with orifice flow is not full") {
        const double target = 500.0;
        run_ramp(detector, 1013.0, target, 1010.0, -5.0, 80, 120, 2.0, 0.0);
        enter_hold(detector, 1013.0, target, 15.0, 0.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 15.0, 0.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION("Deep sealed deadhead trips after 10s") {
        const double target = 213.0;  // 800 mbar
        run_ramp(detector, 1013.0, target, 1010.0, -8.0, 80, 140, 2.0, 40.0);
        enter_hold(detector, 1013.0, target, 2.0, 40.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS), 2.0, 40.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
        err = hold_for(detector, 1013.0, target,
                       samples_for(STABLE_HOLD_DEEP_MS) + 5, 2.0, 40.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Deep hold 180 RPM (full-like G) is full") {
        const double target = 213.0;
        run_ramp(detector, 1013.0, target, 1010.0, -8.0, 80, 140, 2.0, 180.0);
        enter_hold(detector, 1013.0, target, 2.0, 180.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_DEEP_MS) + 5, 2.0, 180.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Deep leak conductance is not full") {
        const double target = 213.0;
        run_ramp(detector, 1013.0, target, 1010.0, -8.0, 80, 140, 2.0, 1100.0);
        enter_hold(detector, 1013.0, target, 2.0, 1100.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_DEEP_MS) + 5, 2.0, 1100.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION("Deep hold 15 mbar overshoot deadhead is full") {
        const double target = 213.0;
        run_ramp(detector, 1013.0, target, 1010.0, -8.0, 80, 140, 2.0, 40.0);
        enter_hold(detector, 1013.0, target, 2.0, 40.0);
        auto err =
            hold_for(detector, 1013.0, target,
                     samples_for(STABLE_HOLD_DEEP_MS) + 5, 2.0, 40.0, -15.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Mid-depth 11 mbar overshoot deadhead is full") {
        const double target = 713.0;  // 300 mbar
        run_ramp(detector, 1013.0, target, 1010.0, -5.0, 80, 120, 2.0, 0.0);
        enter_hold(detector, 1013.0, target, 2.0, 0.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 2.0, 0.0, -11.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("configure() applies zero values") {
        auto cfg = detector.get_config();
        cfg.min_waste_depth_mbar = 0.0;
        cfg.g_sealed_max = 0.0;
        cfg.stable_hold_ms = 0.0;
        detector.configure(cfg);
        auto got = detector.get_config();
        REQUIRE(got.min_waste_depth_mbar == Approx(0.0));
        REQUIRE(got.g_sealed_max == Approx(0.0));
        REQUIRE(got.stable_hold_ms == Approx(0.0));
    }

    SECTION("configure() clamps p_filter_alpha to (0, 1]") {
        auto cfg = detector.get_config();
        cfg.p_filter_alpha = 2.0;
        detector.configure(cfg);
        REQUIRE(detector.get_config().p_filter_alpha == Approx(1.0));

        cfg = detector.get_config();
        cfg.p_filter_alpha = 0.0;
        detector.configure(cfg);
        REQUIRE(detector.get_config().p_filter_alpha == Approx(SENSOR_ALPHA));

        cfg = detector.get_config();
        cfg.p_filter_alpha = 0.25;
        detector.configure(cfg);
        REQUIRE(detector.get_config().p_filter_alpha == Approx(0.25));
    }

    SECTION("Configured G cap is used in hold") {
        auto cfg = detector.get_config();
        cfg.g_sealed_max = 2.0;
        detector.configure(cfg);

        const double target = 313.0;  // 700 mbar
        run_ramp(detector, 1013.0, target, 1010.0, -8.0, 80, 140, 2.0, 800.0);
        enter_hold(detector, 1013.0, target, 2.0, 800.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 2.0, 800.0);
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Configured hold time delays trip") {
        auto cfg = detector.get_config();
        cfg.stable_hold_ms = 20000.0;
        cfg.stable_hold_deep_ms = 20000.0;
        detector.configure(cfg);

        const double target = 500.0;
        run_ramp(detector, 1013.0, target, 1010.0, -5.0, 80, 120, 2.0, 20.0);
        enter_hold(detector, 1013.0, target, 2.0, 20.0);
        auto err = hold_for(detector, 1013.0, target,
                            samples_for(STABLE_HOLD_MS) + 5, 2.0, 20.0);
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION("Pressure oscillation in hold is not full") {
        const double atm = 1013.0;
        const double target = 500.0;
        run_ramp(detector, atm, target, 1010.0, -5.0, 80, 120, 12.0, 400.0);
        enter_hold(detector, atm, target, 5.0, 200.0);

        uint32_t t = 50000;
        for (int i = 0; i < samples_for(STABLE_HOLD_MS) + 5; ++i) {
            t += 40;
            const double p = target + ((i % 12) * 2.0) - 20.0;
            auto err = detector.check(t, p + 5.0, p, target, atm, 400.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }
}

}  // namespace waste_detector
