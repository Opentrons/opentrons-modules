#include <iostream>

#include "catch2/catch.hpp"
#include "flex-stacker/motor_utils.hpp"

using namespace motor_util;

constexpr int MOTOR_FREQUENCY = 100;  // ticks per second

static auto velocity_to_steps_per_tick(double vel) -> sq0_31 {
    return convert_to_fixed_point(vel / static_cast<double>(MOTOR_FREQUENCY),
                                  MovementProfile::radix);
}

static auto acceleration_to_steps_per_tick_sq(double vel) -> sq0_31 {
    return convert_to_fixed_point(
        vel / (static_cast<double>(MOTOR_FREQUENCY * MOTOR_FREQUENCY)),
        MovementProfile::radix);
}

TEST_CASE("Fixed Distance MovementProfile without acceleration") {
    GIVEN("a movement profile with constant velocity") {
        constexpr int velocity = 100;    // steps per second == 1 step / tick
        constexpr int acceleration = 0;  // steps per ticks^2
        constexpr int distance = 10;     // total steps

        auto profile =
            MovementProfile(MOTOR_FREQUENCY, velocity, velocity, acceleration,
                            MovementType::FixedDistance, distance);

        auto expected_velocity = velocity_to_steps_per_tick(velocity);

        THEN("it takes 10 equal steps to be done") {
            for (int i = 0; i < 9; ++i) {
                auto ret = profile.tick();
                REQUIRE(profile.current_velocity() == expected_velocity);
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == false);
            }

            auto ret = profile.tick();
            REQUIRE(ret.step == true);
            REQUIRE(ret.done == true);
        }
    }
}

TEST_CASE("Fixed Distance MovementProfile with acceleration") {
    int initial_velocity =
        GENERATE(0, 1);  // testing both 0 and non-zero initial velocity

    GIVEN(
        "a movement profile with distance long enough to reach max velocity") {
        constexpr int max_velocity = 5;  // steps per tick
        constexpr int acceleration =
            100;  // steps per ticks^2, this acceleration increases the motor
                  // velocity by 1 step per tick
        constexpr int distance = 100;  // total steps

        auto profile = MovementProfile(MOTOR_FREQUENCY, initial_velocity,
                                       max_velocity, acceleration,
                                       MovementType::FixedDistance, distance);

        auto _acceleration_value =
            acceleration_to_steps_per_tick_sq(acceleration);
        auto _max_vel = velocity_to_steps_per_tick(max_velocity);
        // number of ticks to reach max velocity from initial velocity, and vice
        // versa
        auto accel_decel_steps = max_velocity - initial_velocity;

        THEN("the first few ticks are accelerating") {
            for (int i = 0; i < accel_decel_steps; ++i) {
                auto v0 = profile.current_velocity();
                auto ret = profile.tick();
                auto v1 = profile.current_velocity();

                REQUIRE(v1 - v0 == _acceleration_value);
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == false);
            }

            auto accelerated_distance = profile.current_distance();

            AND_THEN("the next phase is coasting at max speed") {
                for (int i = 0;
                     profile.remaining_distance() > accelerated_distance; i++) {
                    auto ret = profile.tick();
                    auto v1 = profile.current_velocity();

                    REQUIRE(v1 == _max_vel);
                    REQUIRE(ret.step == true);
                    REQUIRE(ret.done == false);
                }

                AND_THEN("the last few ticks are decelerating") {
                    for (int i = 0; i < accel_decel_steps - 1; ++i) {
                        auto v0 = profile.current_velocity();
                        auto ret = profile.tick();
                        auto v1 = profile.current_velocity();

                        REQUIRE(v1 - v0 == -1 * _acceleration_value);
                        REQUIRE(ret.step == true);
                        REQUIRE(ret.done == false);
                    }

                    auto ret = profile.tick();
                    REQUIRE(ret.step == true);
                    REQUIRE(ret.done == true);
                }
            }
        }
    }

    GIVEN(
        "a movement profile with distance so short that it doesn't reach max "
        "velocity") {
        constexpr int max_velocity = 4;  // steps per tick
        constexpr int acceleration =
            100;  // steps per ticks^2, this acceleration increases the motor
                  // velocity by 1 step per tick
        constexpr int distance = 4;  // total steps

        auto profile = MovementProfile(MOTOR_FREQUENCY, initial_velocity,
                                       max_velocity, acceleration,
                                       MovementType::FixedDistance, distance);

        auto _acceleration_value =
            acceleration_to_steps_per_tick_sq(acceleration);

        THEN("the first half is accelerating") {
            for (int i = 0; i < distance / 2; ++i) {
                auto v0 = profile.current_velocity();
                static_cast<void>(profile.tick());
                auto v1 = profile.current_velocity();
                REQUIRE(v1 - v0 == _acceleration_value);
            }

            AND_THEN("the second half is decelerating") {
                for (int i = 0; i < distance / 2; ++i) {
                    auto v0 = profile.current_velocity();
                    static_cast<void>(profile.tick());
                    auto v1 = profile.current_velocity();
                    REQUIRE(v1 - v0 == -1 * _acceleration_value);
                }
            }
        }
    }
}
