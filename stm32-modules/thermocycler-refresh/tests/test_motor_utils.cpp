#include "catch2/catch.hpp"
#include "thermocycler-refresh/motor_utils.hpp"

using namespace motor_util;

static auto velocity_to_steps_per_tick(double vel, uint32_t frequency)
    -> sq0_31 {
    return convert_to_fixed_point(vel / static_cast<double>(frequency),
                                  MovementProfile::radix);
}

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

SCENARIO("lid status stringification works") {
    using Status = motor_util::LidStepper::Status;
    std::array<Status, 4> inputs = {Status::BETWEEN, Status::CLOSED,
                                    Status::OPEN, Status::UNKNOWN};
    std::array<std::string, 4> outputs = {"in_between", "closed", "open",
                                          "unknown"};
    WHEN("converting lid status enums to strings ") {
        for (size_t i = 0; i < inputs.size(); ++i) {
            auto result =
                motor_util::LidStepper::status_to_string(inputs.at(i));
            DYNAMIC_SECTION("the string is as expected" << i) {
                REQUIRE_THAT(result, Catch::Matchers::Equals(outputs.at(i)));
            }
        }
    }
}

SCENARIO("seal status stringification works") {
    using Status = motor_util::SealStepper::Status;
    std::array<Status, 4> inputs = {Status::BETWEEN, Status::ENGAGED,
                                    Status::RETRACTED, Status::UNKNOWN};
    std::array<std::string, 4> outputs = {"in_between", "engaged", "retracted",
                                          "unknown"};
    WHEN("converting lid status enums to strings") {
        for (size_t i = 0; i < inputs.size(); ++i) {
            auto result =
                motor_util::SealStepper::status_to_string(inputs.at(i));
            DYNAMIC_SECTION("the string is as expected " << i) {
                REQUIRE_THAT(result, Catch::Matchers::Equals(outputs.at(i)));
            }
        }
    }
}

SCENARIO("MovementProfile functionality with flat acceleration") {
    GIVEN("a movement profile with a 1Hz interrupt") {
        constexpr int frequency = 1;
        WHEN("accelerating instantly from 0 to 1 velocity with distance 1") {
            auto profile = MovementProfile(frequency, 0, 1, 0,
                                           MovementType::FixedDistance, 1);
            THEN("the starting velocity is set to 1 step per tick") {
                REQUIRE(profile.current_velocity() ==
                        velocity_to_steps_per_tick(1.0, frequency));
            }
            THEN("the first tick will result in a step") {
                auto ret = profile.tick();
                REQUIRE(ret.step == true);
                REQUIRE(ret.done == true);
            }
        }
        WHEN("accelerating instantly to 1 velocity with distance 5") {
            auto profile = MovementProfile(frequency, 1, 1, 0,
                                           MovementType::FixedDistance, 5);
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
                MovementProfile(frequency, 0, 1, 0, MovementType::OpenLoop, 1);
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
            auto profile = MovementProfile(frequency, 0, 1, 0,
                                           MovementType::FixedDistance, 1);
            THEN("the starting velocity is set to 0.5 steps per tick") {
                REQUIRE(profile.current_velocity() ==
                        velocity_to_steps_per_tick(1.0F, frequency));
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
            auto profile = MovementProfile(frequency, 1, 1, 0,
                                           MovementType::FixedDistance, 5);
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

SCENARIO("seal stepper utilities work") {
    using namespace motor_util;
    GIVEN("tmc2130 clock speed of 1000Hz") {
        double clock = 1000;
        GIVEN("velocity of 25 steps/sec") {
            double velocity = 25;
            WHEN("converting velocity to period") {
                auto period = SealStepper::velocity_to_tstep(velocity, clock);
                THEN("period is 40 clock ticks") { REQUIRE(period == 40); }
            }
        }
        GIVEN("period of 40 clock ticks") {
            uint32_t period = 40;
            WHEN("converting period to velocity") {
                auto velocity = SealStepper::tstep_to_velocity(period, clock);
                THEN("velocity is 25 steps/sec") {
                    REQUIRE_THAT(velocity, Catch::Matchers::WithinAbs(25, 0.1));
                }
            }
        }
    }
}

SCENARIO("MovementProfile acceleration testing") {
    GIVEN("a movement profile with a 1Hz frequency") {
        constexpr int frequency = 1;
        WHEN("accelerating from 0 to 0.5 at a 0.1 step/sec^2 accel") {
            constexpr double end_vel_double = 0.5;
            constexpr double accel_double = 0.1;
            auto profile =
                MovementProfile(frequency, 0, end_vel_double, accel_double,
                                MovementType::OpenLoop, 10);
            THEN("the starting velocity is 0 steps/tick") {
                REQUIRE(profile.current_velocity() == 0);
            }
            THEN("the velocity increases after a tick") {
                static_cast<void>(profile.tick());
                REQUIRE(static_cast<uint64_t>(profile.current_velocity()) > 0);
            }
            THEN("it takes 5 ticks to reach maximum velocity") {
                const auto end_velocity =
                    velocity_to_steps_per_tick(end_vel_double, frequency);
                for (int i = 0; i < 5; ++i) {
                    REQUIRE(profile.current_velocity() != end_velocity);
                    static_cast<void>(profile.tick());
                }
                // Non-integer math results in a little bit of slop
                REQUIRE(std::abs(profile.current_velocity() - end_velocity) <
                        5);
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
            auto profile =
                MovementProfile(frequency, 0, end_vel_double, accel_double,
                                MovementType::OpenLoop, 10);
            THEN("the velocity increases after a tick") {
                static_cast<void>(profile.tick());
                REQUIRE(static_cast<uint64_t>(profile.current_velocity()) > 0);
            }
            THEN("it takes 1000 ticks to reach maximum velocity") {
                const auto end_velocity =
                    velocity_to_steps_per_tick(end_vel_double, frequency);
                for (int i = 0; i < frequency + 1; ++i) {
                    REQUIRE(profile.current_velocity() != end_velocity);
                    static_cast<void>(profile.tick());
                }
                REQUIRE(profile.current_velocity() == end_velocity);
            }
        }
    }
    GIVEN("a movement profile with a 1MHz frequency") {
        constexpr int frequency = 1000000;
        WHEN("accelerating from 0 to 50000 at a 50000 step/sec^2 accel") {
            constexpr double end_vel_double = 50000.0F;
            constexpr double accel_double = 50000.0F;
            auto profile =
                MovementProfile(frequency, 0, end_vel_double, accel_double,
                                MovementType::OpenLoop, 10);
            THEN("the velocity increases after a tick") {
                static_cast<void>(profile.tick());
                REQUIRE(static_cast<uint64_t>(profile.current_velocity()) > 0);
            }
            THEN("it takes 1000000 ticks to reach maximum velocity") {
                const auto end_velocity =
                    velocity_to_steps_per_tick(end_vel_double, frequency);
                auto vel = profile.current_velocity();
                int ticks = 0;
                while (vel < end_velocity) {
                    profile.tick();
                    vel = profile.current_velocity();
                    ++ticks;
                }
                // Some slop due to floating point math and small numbers
                REQUIRE(std::abs(ticks - frequency) < 4000);
            }
        }
    }
}

SCENARIO("MovementProfile input sanitization") {
    WHEN("setting start velocity below zero") {
        auto profile =
            MovementProfile(1, -1, 10, 0.1, MovementType::FixedDistance, 10);
        THEN("the velocity is clamped up to 0") {
            REQUIRE(profile.current_velocity() == 0);
        }
    }
    WHEN("setting end velocity below start velocity") {
        auto profile =
            MovementProfile(1, 0.5, 0, 0.1, MovementType::FixedDistance, 10);
        THEN("the end velocity gets set to the starting velocity") {
            const int velocity = velocity_to_steps_per_tick(0.5, 1);
            REQUIRE(profile.current_velocity() == velocity);
            static_cast<void>(profile.tick());
            REQUIRE(profile.current_velocity() == velocity);
        }
    }
    WHEN("setting acceleration below zero") {
        auto profile =
            MovementProfile(1, 0.5, 0.75, -5, MovementType::FixedDistance, 10);
        THEN("the velocity behaves as if the acceleration was passed as 0") {
            const int velocity = velocity_to_steps_per_tick(0.75, 1);
            REQUIRE(profile.current_velocity() == velocity);
            static_cast<void>(profile.tick());
            REQUIRE(profile.current_velocity() == velocity);
        }
    }
}
