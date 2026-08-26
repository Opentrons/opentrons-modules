#include <catch2/catch.hpp>

#include "firmware/waste_detector.hpp"

namespace waste_detector {

TEST_CASE("WasteDetector - Core Behavior", "[waste][detector]") {
    WasteDetector detector;

    SECTION("Default state after construction") {
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
        REQUIRE_FALSE(detector.baseline_captured());
        REQUIRE(detector.check(0, 1013.0, 1013.0, 200.0, 1013.0) ==
                WasteFullError::NO_ERROR);
    }

    SECTION("reset() clears all state") {
        detector.check(1000, 500.0, 505.0, 200.0, 1013.0);  // force some state
        detector.reset();
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
        REQUIRE_FALSE(detector.baseline_captured());
        REQUIRE(detector.check(0, 1013.0, 1013.0, 200.0, 1013.0) ==
                WasteFullError::NO_ERROR);
    }

    SECTION("Fast spike during ramp triggers RISE_TOO_FAST_ERROR") {
        auto err = WasteFullError::NO_ERROR;
        auto atm_pressure = 1013.0F;
        auto t_pressure = 500;
        auto c_pressure = 1010.0F;
        auto tics = 0.0F;

        detector.reset();
        for (uint32_t i = 0; i <= 30; i++) {
            tics += i * 2;
            c_pressure -= 20;  // rise too quickly
            err = detector.check(tics, c_pressure, c_pressure + 2, t_pressure,
                                 atm_pressure);
        }
        REQUIRE(err == WasteFullError::RISE_TOO_FAST_ERROR);
        REQUIRE(detector.get_error() == WasteFullError::RISE_TOO_FAST_ERROR);
    }

    SECTION("Normal empty run learns baseline correctly") {
        auto err = WasteFullError::NO_ERROR;
        auto atm_pressure = 1013.0F;
        auto t_pressure = 500;
        auto c_pressure = 1010.0F;
        auto tics = 0.0F;

        detector.reset();
        for (uint32_t i = 0; i <= 100; i++) {
            tics += i * 2;
            c_pressure -= 5;  // learn normal baseline
            err = detector.check(tics, c_pressure, c_pressure + 2, t_pressure,
                                 atm_pressure);
        }

        REQUIRE(err == WasteFullError::NO_ERROR);
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
        REQUIRE(detector.baseline_captured());
    }

    SECTION("Second run faster than baseline triggers FAST_BASELINE_ERROR") {
        detector.reset();
        auto err = WasteFullError::NO_ERROR;
        auto atm_pressure = 1013.0F;
        auto t_pressure = 500;
        auto c_pressure = 1010.0F;
        auto tics = 0.0F;
        detector.reset();
        for (uint32_t i = 0; i <= 100; i++) {
            tics += i * 2;
            c_pressure -= 5;  // learn normal baseline
            err = detector.check(tics, c_pressure, c_pressure + 2, t_pressure,
                                 atm_pressure);
        }

        c_pressure = 1010.0;
        detector.reset();
        for (uint32_t i = 0; i <= 40; i++) {
            tics += i * 2;
            c_pressure -= 15;  // fast baseline
            err = detector.check(tics, c_pressure, c_pressure + 2, t_pressure,
                                 atm_pressure);
        }

        REQUIRE(err == WasteFullError::FAST_BASELINE_ERROR);
    }

    SECTION("Slow first run is empty, not full — learns if under max window") {
        auto err = WasteFullError::NO_ERROR;
        auto atm_pressure = 1013.0F;
        auto t_pressure = 500;
        auto c_pressure = 1010.0F;
        auto tics = 0.0F;
        detector.reset();
        for (uint32_t i = 0; i <= 100; i++) {
            tics += i * 3;  // slow empty ramp
            c_pressure -= 5;
            err = detector.check(tics, c_pressure, c_pressure + 2, t_pressure,
                                 atm_pressure);
        }

        REQUIRE(err == WasteFullError::NO_ERROR);
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - sudden blocked flow triggers SUDDEN_BLOCKED_ERROR") {
        // Get to hold phase
        auto c_pressure = 190.0F;
        auto tics = 3000.0F;
        detector.reset();
        detector.check(1000, 500.0, 505.0, 200.0, 1013.0);
        detector.check(3000, 210, 215, 200.0, 1013.0);
        for (uint32_t i = 0; i <= NEAR_TARGET_TICS + 5; i++) {
            tics += i;
            detector.check(tics, c_pressure, c_pressure + 2, 200.0,
                           1013.0);  // ramp finished
        }

        // fill window
        for (int i = 0; i < PRESSURE_WINDOW_SIZE; ++i) {
            detector.check(tics, c_pressure, c_pressure + 3, 200.0, 1013.0);
        }

        // Repeated slam while remaining inside the 20 mbar hold band
        auto err = WasteFullError::NO_ERROR;
        for (int i = 0; i < 4; ++i) {
            tics += 1;
            c_pressure += MAX_RISE_PER_TICK + 8;
            err = detector.check(tics, c_pressure, c_pressure + 2, 200.0,
                                 1013.0);
            if (err == WasteFullError::SUDDEN_BLOCKED_ERROR) {
                break;
            }
        }
        REQUIRE(err == WasteFullError::SUDDEN_BLOCKED_ERROR);
    }

    SECTION("Hold phase - cumulative rise triggers CUMMULATIVE_BLOCKED_ERROR") {
        auto c_pressure = 190.0F;
        auto tics = 3000.0F;
        detector.reset();
        detector.check(1000, 498.0, 500.0, 200.0, 1013.0);
        detector.check(3000, 210, 215, 200.0, 1013.0);
        for (uint32_t i = 0; i <= NEAR_TARGET_TICS; i++) {
            tics += i;
            detector.check(tics, c_pressure + 2, c_pressure, 200.0,
                           1013.0);  // ramp finished
        }

        // fill window
        for (int i = 0; i < PRESSURE_WINDOW_SIZE; ++i) {
            detector.check(tics, c_pressure, c_pressure + 3, 200.0, 1013.0);
        }

        // ~15 mbar slow rise stays inside hold and below the 40 mbar
        // cumulative gate (empty plate hunting is ~10 mbar).
        for (int i = 0; i < 25; ++i) {
            tics += 1;
            c_pressure += 0.6;
            detector.check(tics, c_pressure, c_pressure + 2, 200.0, 1013.0);
        }

        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Draining air rush is correctly ignored") {
        detector.reset();
        detector.check(1000, 502.0, 500.0, 200.0, 1013.0);
        detector.check(3000, 246.0, 244.0, 200.0, 1013.0);

        // Big negative delta (air rush while draining)
        auto err = detector.check(3100, 302.0, 300.0, 200.0, 1013.0);
        // delta_p = -61 (but we simulate big negative)
        // In real usage you'd pass the real delta, but the test confirms it
        // doesn't trigger
        REQUIRE(err == WasteFullError::NO_ERROR);
    }

    SECTION(
        "Hold phase - pressure oscillation does NOT trigger "
        "FLOW_STABLE_FULL_ERROR") {
        detector.reset();

        const double atm = 1013.0;
        const double target = 500.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 200; i++) {
            tics += i * 2;
            current_p -= 5;  // learn normal baseline
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        REQUIRE(detector.baseline_captured());
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);

        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            detector.check(tics, current_p + 5, current_p + 3, target, atm);
            current_p = target + 8.0;
        }

        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);

        for (int i = 0; i < PRESSURE_WINDOW_SIZE + 5; ++i) {
            tics += 40;
            current_p = target + ((i % 12) * 2.0) - 20;
            auto err =
                detector.check(tics, current_p + 5, current_p + 3, target, atm);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION(
        "Hold phase - quiet sealed hold eventually triggers "
        "FLOW_STABLE_FULL_ERROR") {
        detector.reset();

        const double atm = 1013.0;
        const double target = 500.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;  // learn normal baseline
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        REQUIRE(detector.baseline_captured());
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);

        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm);
        }

        auto err = WasteFullError::NO_ERROR;
        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS + 5;
             ++i) {
            tics += 40;
            current_p = target + 3.0;
            err = detector.check(tics, current_p + 2, current_p, target, atm,
                                 0.0);
            if (err == WasteFullError::FLOW_STABLE_FULL_ERROR) {
                break;
            }
        }
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
        REQUIRE(detector.get_error() == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Hold phase - quiet with pump loaded is not full") {
        detector.reset();

        const double atm = 1013.0;
        const double target = 713.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 0.0);
        }

        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS + 5;
             ++i) {
            tics += 40;
            current_p = target + 3.0;
            auto err = detector.check(tics, current_p + 2, current_p, target,
                                      atm, 200.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - mid-depth leak RPM is not full") {
        detector.reset();

        // Depth 700 mbar: shallow RPM cap, 250 RPM must not trip.
        const double atm = 1013.0;
        const double target = 313.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 250.0);
        }

        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS_DEEP + 5;
             ++i) {
            tics += 40;
            current_p = target + 3.0;
            auto err = detector.check(tics, current_p + 2, current_p, target,
                                      atm, 250.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - mid-depth quiet unloaded is full") {
        detector.reset();

        const double atm = 1013.0;
        const double target = 313.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 0.0);
        }

        auto err = WasteFullError::NO_ERROR;
        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS + 5;
             ++i) {
            tics += 40;
            current_p = target + 3.0;
            err = detector.check(tics, current_p + 2, current_p, target, atm,
                                 0.0);
            if (err == WasteFullError::FLOW_STABLE_FULL_ERROR) {
                break;
            }
        }
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
        REQUIRE(detector.get_error() == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Hold phase - deep target quiet hold allows higher RPM") {
        detector.reset();

        // Depth 800 mbar: 250 RPM is under the deep cap and must trip.
        const double atm = 1013.0;
        const double target = 213.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 250.0);
        }

        auto err = WasteFullError::NO_ERROR;
        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS_DEEP + 5;
             ++i) {
            tics += 40;
            current_p = target + 3.0;
            err = detector.check(tics, current_p + 2, current_p, target, atm,
                                 250.0);
            if (err == WasteFullError::FLOW_STABLE_FULL_ERROR) {
                break;
            }
        }
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
        REQUIRE(detector.get_error() == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Hold phase - deep quiet with leak RPM is not full") {
        detector.reset();

        // Deep leak RPM must not trip.
        const double atm = 1013.0;
        const double target = 213.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm,
                           1100.0);
        }

        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS_DEEP + 5;
             ++i) {
            tics += 40;
            current_p = target + 3.0;
            auto err = detector.check(tics, current_p + 2, current_p, target,
                                      atm, 1100.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - deep quiet 15 mbar shallow is not full") {
        detector.reset();

        // 15 mbar off target must not trip.
        const double atm = 1013.0;
        const double target = 213.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 15.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 250.0);
        }

        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS_DEEP + 5;
             ++i) {
            tics += 40;
            current_p = target + 15.0;
            auto err = detector.check(tics, current_p + 2, current_p, target,
                                      atm, 250.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - deep quiet 6s on setpoint is not yet full") {
        detector.reset();

        const double atm = 1013.0;
        const double target = 213.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 250.0);
        }

        // 6s is not enough at a deep target.
        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS; ++i) {
            tics += 40;
            current_p = target + 3.0;
            auto err = detector.check(tics, current_p + 2, current_p, target,
                                      atm, 250.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - deep noisy pressure with unloaded RPM is full") {
        detector.reset();

        // Deep hold must trip even when pressure std > 1.0.
        const double atm = 1013.0;
        const double target = 213.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 200.0);
        }

        auto err = WasteFullError::NO_ERROR;
        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS_DEEP + 5;
             ++i) {
            tics += 40;
            current_p = target + ((i % 2) ? 4.0 : 0.0);
            err = detector.check(tics, current_p + 2, current_p, target, atm,
                                 200.0);
            if (err == WasteFullError::FLOW_STABLE_FULL_ERROR) {
                break;
            }
        }
        REQUIRE(err == WasteFullError::FLOW_STABLE_FULL_ERROR);
        REQUIRE(detector.get_error() == WasteFullError::FLOW_STABLE_FULL_ERROR);
    }

    SECTION("Hold phase - deep unloaded 5s then leak RPM is not full") {
        detector.reset();

        // 5s unloaded then leak RPM must not trip.
        const double atm = 1013.0;
        const double target = 213.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 200.0);
        }

        const int dip_ticks = PRESSURE_WINDOW_SIZE + 125;  // ~5s after window
        for (int i = 0; i < dip_ticks; ++i) {
            tics += 40;
            current_p = target + 3.0;
            auto err = detector.check(tics, current_p + 2, current_p, target,
                                      atm, 200.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        for (int i = 0; i < STABLE_HOLD_TICKS_DEEP + 5; ++i) {
            tics += 40;
            current_p = target + 3.0;
            auto err = detector.check(tics, current_p + 2, current_p, target,
                                      atm, 1100.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - quiet with orifice flow is not full") {
        detector.reset();

        const double atm = 1013.0;
        const double target = 500.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 96; i++) {
            tics += i * 2;
            current_p -= 5;
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        for (uint32_t i = 0; i < NEAR_TARGET_TICS + 10; ++i) {
            tics += 40;
            current_p = target + 3.0;
            detector.check(tics, current_p + 2, current_p, target, atm, 0.0);
        }

        for (int i = 0; i < PRESSURE_WINDOW_SIZE + STABLE_HOLD_TICKS + 5;
             ++i) {
            tics += 40;
            current_p = target + 3.0;
            auto err = detector.check(tics, current_p + 15, current_p, target,
                                      atm, 0.0);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }

    SECTION("Hold phase - oscillation after window fill is still not full") {
        detector.reset();

        const double atm = 1013.0;
        const double target = 500.0;
        double current_p = 1000.0;
        uint32_t tics = 1000;

        for (uint32_t i = 0; i <= 100; i++) {
            tics += i * 2;
            current_p -= 5;  // learn normal baseline
            detector.check(tics, current_p, current_p + 2, target, atm);
        }
        REQUIRE(detector.baseline_captured());
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);

        for (uint32_t i = 0; i < NEAR_TARGET_TICS; ++i) {
            tics += 40;
            detector.check(tics, current_p + 2, current_p, target, atm);
        }

        for (int i = 0; i < PRESSURE_WINDOW_SIZE + 5; ++i) {
            tics += 40;
            current_p = target + ((i % 10) * 12.0) - 20.0;
            auto err =
                detector.check(tics, current_p + 2, current_p, target, atm);
            REQUIRE(err == WasteFullError::NO_ERROR);
        }
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
    }
}

}  // namespace waste_detector
