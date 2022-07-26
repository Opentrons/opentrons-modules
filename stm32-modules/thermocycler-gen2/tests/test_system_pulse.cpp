#include <array>

#include "catch2/catch.hpp"
#include "thermocycler-gen2/system_task.hpp"

TEST_CASE("Heartbeat class functionality") {
    GIVEN("a heartbeat with a period of 5") {
        const uint32_t PERIOD = 5;
        auto subject = system_task::Pulse(PERIOD);
        THEN("the pwm starts at 0") { REQUIRE(subject.pwm() == 0); }
        THEN("the first two PERIOD worth of ticks are all off") {
            for (size_t i = 0; i < PERIOD * 2; ++i) {
                DYNAMIC_SECTION("tick " + i) {
                    for (size_t j = 0; j < i; ++j) {
                        subject.tick();
                    }
                    REQUIRE(!subject.tick());
                }
            }
        }
        THEN("the pwm changes every 5 ticks") {
            auto values = std::array<uint8_t, 14>{
                {0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0, 1, 2, 3}};
            for (size_t i = 0; i < values.size(); ++i) {
                DYNAMIC_SECTION("pwm value: " + values[i]) {
                    REQUIRE(subject.pwm() == 0);
                    for (size_t j = 0; j < PERIOD * i; ++j) {
                        subject.tick();
                    }
                    REQUIRE(subject.pwm() == values[i]);
                }
            }
        }
        AND_GIVEN("the heartbeat has been ticked to a PWM of 3") {
            while (subject.pwm() < 3) {
                subject.tick();
            }
            THEN("the next two ticks should turn on the LED") {
                REQUIRE(subject.tick());
                REQUIRE(subject.tick());
                AND_THEN("the next tick should turn the LED off") {
                    REQUIRE(!subject.tick());
                }
            }
        }
    }
}

TEST_CASE("FrontButtonBlink class functionality") {
    auto subject = system_task::FrontButtonBlink();
    WHEN("ticking through the entire sequence") {
        auto intended = std::vector<bool>();
        for (int i = 0; i < 200; ++i) {
            intended.push_back(false);
        }
        for (int i = 0; i < 199; ++i) {
            intended.push_back(true);
        }
        for (int i = 0; i < 201; ++i) {
            intended.push_back(false);
        }
        for (int i = 0; i < 1000; ++i) {
            intended.push_back(true);
        }
        auto results = std::vector<bool>();
        while (results.size() < intended.size()) {
            results.push_back(subject.tick());
        }
        THEN("the results match") {
            REQUIRE_THAT(results, Catch::Matchers::Equals(intended));
        }
    }
}
