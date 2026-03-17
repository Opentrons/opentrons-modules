#include <catch2/catch.hpp>

#include "firmware/waste_detector.hpp"

namespace waste_detector {

TEST_CASE("WasteDetector - Core Behavior", "[waste][detector]") {
    WasteDetector detector;

    SECTION("Default state after construction") {
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
        REQUIRE_FALSE(detector.baseline_captured());
        REQUIRE(detector.check(0, 1013.0, 200.0, 1013.0) ==
                WasteFullError::NO_ERROR);
    }

    SECTION("reset() clears all state") {
        detector.check(1000, 500.0, 200.0, 1013.0);  // force some state
        detector.reset();
        REQUIRE(detector.get_error() == WasteFullError::NO_ERROR);
        REQUIRE_FALSE(detector.baseline_captured());
        REQUIRE(detector.check(0, 1013.0, 200.0, 1013.0) ==
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
            err = detector.check(tics, c_pressure, t_pressure, atm_pressure);
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
            err = detector.check(tics, c_pressure, t_pressure, atm_pressure);
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
            err = detector.check(tics, c_pressure, t_pressure, atm_pressure);
        }

        c_pressure = 1010.0;
        detector.reset();
        for (uint32_t i = 0; i <= 40; i++) {
            tics += i * 2;
            c_pressure -= 15;  // fast baseline
            err = detector.check(tics, c_pressure, t_pressure, atm_pressure);
        }

        REQUIRE(err == WasteFullError::FAST_BASELINE_ERROR);
    }

    SECTION("First run too slow triggers FIRST_RUN_SLOW_ERROR") {
        auto err = WasteFullError::NO_ERROR;
        auto atm_pressure = 1013.0F;
        auto t_pressure = 500;
        auto c_pressure = 1010.0F;
        auto tics = 0.0F;
        detector.reset();
        for (uint32_t i = 0; i <= 100; i++) {
            tics += i * 3;  // slow baseline
            c_pressure -= 5;
            err = detector.check(tics, c_pressure, t_pressure, atm_pressure);
        }

        REQUIRE(err == WasteFullError::FIRST_RUN_SLOW_ERROR);
    }

    SECTION("Hold phase - sudden blocked flow triggers SUDDEN_BLOCKED_ERROR") {
        // Get to hold phase
        auto c_pressure = 190.0F;
        auto tics = 3000.0F;
        detector.reset();
        detector.check(1000, 500.0, 200.0, 1013.0);
        detector.check(3000, 210, 200.0, 1013.0);
        for (uint32_t i = 0; i <= NEAR_TARGET_TICS + 5; i++) {
            tics += i;
            detector.check(tics, c_pressure, 200.0, 1013.0);  // ramp finished
        }

        // Now in hold - sudden rise
        tics += 1;
        c_pressure += MAX_RISE_PER_TICK + 5;
        auto err = detector.check(tics, c_pressure, 200.0, 1013.0);
        REQUIRE(err == WasteFullError::SUDDEN_BLOCKED_ERROR);
    }

    SECTION("Hold phase - cumulative rise triggers CUMMULATIVE_BLOCKED_ERROR") {
        auto c_pressure = 190.0F;
        auto tics = 3000.0F;
        detector.reset();
        detector.check(1000, 500.0, 200.0, 1013.0);
        detector.check(3000, 210, 200.0, 1013.0);
        for (uint32_t i = 0; i <= NEAR_TARGET_TICS + 5; i++) {
            tics += i;
            detector.check(tics, c_pressure, 200.0, 1013.0);  // ramp finished
        }

        // Slow gradual rise over many ticks
        for (int i = 0; i < 20; ++i) {
            tics += i;
            c_pressure += 0.6;
            detector.check(tics, c_pressure, 200.0, 1013.0);
        }

        REQUIRE(detector.get_error() ==
                WasteFullError::CUMMULATIVE_BLOCKED_ERROR);
    }

    SECTION("Draining air rush is correctly ignored") {
        detector.reset();
        detector.check(1000, 500.0, 200.0, 1013.0);
        detector.check(3000, 239.0, 200.0, 1013.0);

        // Big negative delta (air rush while draining)
        auto err = detector.check(3100, 300.0, 200.0, 1013.0);
        // delta_p = -61 (but we simulate big negative)
        // In real usage you'd pass the real delta, but the test confirms it
        // doesn't trigger
        REQUIRE(err == WasteFullError::NO_ERROR);
    }
}

}  // namespace waste_detector
