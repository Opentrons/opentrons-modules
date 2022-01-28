#include "catch2/catch.hpp"
#include "thermocycler-refresh/motor_utils.hpp"

using namespace motor_util;

SCENARIO("Lid stepper current conversion works") {
    GIVEN("a target current 8.25 amps") {
        double current = 8250.0;
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
    GIVEN("a target current 10 A") {
        double current = 10000.0;
        WHEN("converting") {
            auto result = LidStepper::current_to_dac(current);
            THEN("result should be clamped down to max DAC value") {
                REQUIRE(result == 0xFF);
            }
        }
    }
    GIVEN("a target current 8.25/2 A") {
        double current = 8250.0 / 2.0;
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

SCENARIO("MovementProfile functionality with flat acceleration") {
    constexpr int radix = MovementProfile<1>::radix;
    GIVEN("a movement profile with a 1Hz interrupt") {
        constexpr int frequency = 1;
        WHEN("accelerating instantly from 0 to 1 velocity with distance 1") {
            auto profile = MovementProfile<frequency>(
                0, 1, 0, MovementType::FixedDistance, 1);
            THEN("the starting velocity is set to 1 step per tick") {
                REQUIRE(profile.current_velocity() ==
                        convert_to_fixed_point(1.0 / frequency, radix));
            }
            THEN("the first tick will result in a step") {
                auto ret = profile.tick();
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == true);
            }
        }
        WHEN("accelerating instantly to 1 velocity with distance 5") {
            auto profile = MovementProfile<frequency>(
                1, 1, 0, MovementType::FixedDistance, 5);
            THEN("it takes 5 ticks to be done") {
                for (int i = 0; i < 4; ++i) {
                    auto ret = profile.tick();
                    REQUIRE(ret.step == true);
                    REQUIRE(ret.done == false);
                }
                auto ret = profile.tick();
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == true);
            }
        }
        WHEN("running open loop") {
            auto profile =
                MovementProfile<frequency>(0, 1, 0, MovementType::SoftStop, 1);
            THEN("the distance is ignored") {
                auto ret = profile.tick();
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == false);
                ret = profile.tick();
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == false);
            }
        }
    }
    GIVEN("a movement profile with a 2Hz interrupt") {
        constexpr int frequency = 2;
        WHEN("accelerating instantly from 0 to 1 velocity with distance 1") {
            auto profile = MovementProfile<frequency>(
                0, 1, 0, MovementType::FixedDistance, 1);
            THEN("the starting velocity is set to 0.5 steps per tick") {
                REQUIRE(profile.current_velocity() ==
                        convert_to_fixed_point(1.0 / frequency, radix));
            }
            THEN("the first tick will not result in a step") {
                auto ret = profile.tick();
                REQUIRE(ret.step == false);
                REQUIRE(ret.done == false);
                AND_THEN("the second tick will result in a step") {
                    auto ret = profile.tick();
                    REQUIRE(ret.step == true);
                    REQUIRE(ret.done == true);
                }
            }
        }
        WHEN("accelerating instantly to 1 velocity with distance 5") {
            auto profile = MovementProfile<frequency>(
                1, 1, 0, MovementType::FixedDistance, 5);
            THEN("it takes 10 ticks to be done") {
                for (int i = 0; i < 4; ++i) {
                    auto ret = profile.tick();
                    REQUIRE(ret.step == false);
                    REQUIRE(ret.done == false);
                    ret = profile.tick();
                    REQUIRE(ret.step == true);
                    REQUIRE(ret.done == false);
                }
                auto ret = profile.tick();
                REQUIRE(ret.step == false);
                REQUIRE(ret.done == false);
                ret = profile.tick();
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == true);
            }
        }
    }
}

SCENARIO("MovementProfile acceleration testing") {
    constexpr int radix = MovementProfile<1>::radix;
    GIVEN("a movement profile with a 1Hz frequency") {
        constexpr int frequency = 1;
        WHEN("accelerating from 0 to 0.5 at a 0.1 step/sec^2 accel") {
            constexpr double end_vel_double = 0.5;
            constexpr double accel_double = 0.1;
            auto profile = MovementProfile<frequency>(
                0, end_vel_double, accel_double, MovementType::SoftStop, 10);
            THEN("the starting velocity is 0 steps/tick") {
                REQUIRE(profile.current_velocity() == 0);
            }
            THEN("the velocity increases after a tick") {
                static_cast<void>(profile.tick());
                REQUIRE(static_cast<uint64_t>(profile.current_velocity()) > 0);
            }
            THEN("it takes 5 ticks to reach maximum velocity") {
                const auto end_velocity =
                    convert_to_fixed_point(end_vel_double / frequency, radix);
                for (int i = 0; i < 5; ++i) {
                    REQUIRE(profile.current_velocity() != end_velocity);
                    static_cast<void>(profile.tick());
                }
                REQUIRE(profile.current_velocity() == end_velocity);
                AND_THEN("more ticks do not increase the velocity") {
                    static_cast<void>(profile.tick());
                    REQUIRE(profile.current_velocity() == end_velocity);
                }
            }
        }
    }
    GIVEN("a movement profile with a 1000Hz frequency") {
        constexpr int frequency = 1000;
        WHEN("accelerating from 0 to 100 at a 100 step/sec^2 accel") {
            constexpr double end_vel_double = 100;
            constexpr double accel_double = 100;
            auto profile = MovementProfile<frequency>(
                0, end_vel_double, accel_double, MovementType::SoftStop, 10);
            THEN("the velocity increases after a tick") {
                static_cast<void>(profile.tick());
                REQUIRE(static_cast<uint64_t>(profile.current_velocity()) > 0);
            }
            THEN("it takes 1000 ticks to reach maximum velocity") {
                const auto end_velocity =
                    convert_to_fixed_point(end_vel_double / frequency, radix);
                for (int i = 0; i < frequency + 1; ++i) {
                    REQUIRE(profile.current_velocity() != end_velocity);
                    static_cast<void>(profile.tick());
                }
                REQUIRE(profile.current_velocity() == end_velocity);
            }
        }
    }
}

SCENARIO("MovementProfile input sanitization") {
    constexpr int radix = MovementProfile<1>::radix;
    WHEN("setting start velocity below zero") {
        auto profile =
            MovementProfile<1>(-1, 10, 0.1, MovementType::FixedDistance, 10);
        THEN("the velocity is clamped up to 0") {
            REQUIRE(profile.current_velocity() == 0);
        }
    }
    WHEN("setting end velocity below start velocity") {
        auto profile =
            MovementProfile<1>(0.5, 0, 0.1, MovementType::FixedDistance, 10);
        THEN("the end velocity gets set to the starting velocity") {
            const int velocity = convert_to_fixed_point(0.5, radix);
            REQUIRE(profile.current_velocity() == velocity);
            static_cast<void>(profile.tick());
            REQUIRE(profile.current_velocity() == velocity);
        }
    }
    WHEN("setting acceleration below zero") {
        auto profile =
            MovementProfile<1>(0.5, 0.75, -5, MovementType::FixedDistance, 10);
        THEN("the velocity behaves as if the acceleration was passed as 0") {
            const int velocity = convert_to_fixed_point(0.75, radix);
            REQUIRE(profile.current_velocity() == velocity);
            static_cast<void>(profile.tick());
            REQUIRE(profile.current_velocity() == velocity);
        }
    }
}
