#include "catch2/catch.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/motor_task.hpp"
#include "test/task_builder.hpp"

SCENARIO("motor task core message handling", "[motor]") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        WHEN("just having been built") {
            THEN("the state should be idle/unknown") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::STOPPED_UNKNOWN);
            }
        }

        WHEN("sending a set-rpm message as if from the host comms") {
            auto message =
                messages::SetRPMMessage{.id = 222, .target_rpm = 1254};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
                AND_THEN("the task should set the rpm and disengage solenoid") {
                    REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 1254);
                }
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    REQUIRE(tasks->get_system_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto ack =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(ack.responding_to_id == message.id);
                }
                AND_THEN("the task state should be running") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::RUNNING);
                }
            }
        }
        WHEN("sending a set-rpm message as if from the system") {
            auto message = messages::SetRPMMessage{
                .id = 222, .target_rpm = 1254, .from_system = true};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
                AND_THEN(
                    "the task should set the rpm and disengage the solenoid") {
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 1254);
                    REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
                }
                AND_THEN(
                    "the task should respond to the message to the system") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.empty());
                    REQUIRE(!tasks->get_system_queue().backing_deque.empty());
                    auto response =
                        tasks->get_system_queue().backing_deque.front();
                    tasks->get_system_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto ack =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(ack.responding_to_id == message.id);
                }
                AND_THEN("the task state should be running") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::RUNNING);
                }
            }
        }
        WHEN("sending a get-rpm message") {
            auto message = messages::GetRPMMessage{.id = 123};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_policy().test_set_current_rpm(1050);
            tasks->get_motor_policy().set_rpm(3500);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
                AND_THEN("the task should respond to the message") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::GetRPMResponse>(
                        response));
                    auto getrpm = std::get<messages::GetRPMResponse>(response);
                    REQUIRE(getrpm.responding_to_id == message.id);
                    REQUIRE(getrpm.current_rpm == 1050);
                    REQUIRE(getrpm.setpoint_rpm == 3500);
                }
            }
        }

        WHEN("sending a set-acceleration message") {
            auto message =
                messages::SetAccelerationMessage{.id = 123, .rpm_per_s = 9999};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
                REQUIRE(tasks->get_motor_policy().test_get_ramp_rate() ==
                        message.rpm_per_s);
                AND_THEN("the task should respond to the message") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto ack =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(ack.responding_to_id == message.id);
                }
            }
        }
    }
}

SCENARIO("motor task error handling", "[motor]") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::STOPPED_UNKNOWN);
        WHEN("sending an internal error with one bit set") {
            auto message = messages::MotorSystemErrorMessage{
                .errors = static_cast<uint16_t>(
                    1u << errors::MotorErrorOffset::SW_ERROR)};
            tasks->get_motor_queue().backing_deque.push_back(message);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());

            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
                AND_THEN(
                    "the task should send one error message to host comms") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.size() ==
                        1);
                    auto upstream =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::ErrorMessage>(
                        upstream));
                    REQUIRE(std::get<messages::ErrorMessage>(upstream).code ==
                            errors::ErrorCode::MOTOR_BLDC_DRIVER_ERROR);
                }
                AND_THEN("the task should enter error state") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::ERROR);
                }
            }
        }
        WHEN("sending an internal error with multiple bits set") {
            auto message = messages::MotorSystemErrorMessage{
                .errors = static_cast<uint16_t>(
                    (1u << errors::MotorErrorOffset::OVERCURRENT) |
                    (1u << errors::MotorErrorOffset::FOC_DURATION) |
                    (1u << errors::MotorErrorOffset::UNDER_VOLT))};
            tasks->get_motor_queue().backing_deque.push_back(message);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
                AND_THEN(
                    "the task should send one error message for each sent bit "
                    "to host comms") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.size() ==
                        3);
                    auto foc_duration_msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::ErrorMessage>(
                        foc_duration_msg));
                    REQUIRE(std::get<messages::ErrorMessage>(foc_duration_msg)
                                .code == errors::ErrorCode::MOTOR_FOC_DURATION);
                    auto undervolt_msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::ErrorMessage>(
                        undervolt_msg));
                    REQUIRE(
                        std::get<messages::ErrorMessage>(undervolt_msg).code ==
                        errors::ErrorCode::MOTOR_BLDC_UNDERVOLT);
                    auto overcurrent_msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::ErrorMessage>(
                        overcurrent_msg));
                    REQUIRE(std::get<messages::ErrorMessage>(overcurrent_msg)
                                .code ==
                            errors::ErrorCode::MOTOR_BLDC_OVERCURRENT);
                }
                AND_THEN("the task should enter error state") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::ERROR);
                }
            }
        }

        WHEN("sending an internal error with no bits set") {
            auto message = messages::MotorSystemErrorMessage();
            tasks->get_motor_queue().backing_deque.push_back(message);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
                AND_THEN(
                    "the task should send a spurious-error message upstream") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.size() ==
                        1);
                    auto spurious_msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::ErrorMessage>(
                        spurious_msg));
                    REQUIRE(
                        std::get<messages::ErrorMessage>(spurious_msg).code ==
                        errors::ErrorCode::MOTOR_SPURIOUS_ERROR);
                }
                AND_THEN("the task should not enter error state") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::STOPPED_UNKNOWN);
                }
            }
        }
    }
}

SCENARIO("motor task input error handling", "[motor]") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        WHEN("a command requests an invalid speed") {
            tasks->get_motor_policy().test_set_rpm_return_code(
                errors::ErrorCode::MOTOR_ILLEGAL_SPEED);
            auto message =
                messages::SetRPMMessage{.id = 123, .target_rpm = 9999};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the motor task should respond with an error") {
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                auto ack = std::get<messages::AcknowledgePrevious>(response);
                REQUIRE(ack.with_error ==
                        errors::ErrorCode::MOTOR_ILLEGAL_SPEED);
            }
        }
        WHEN("a command requests an invalid ramp rate") {
            tasks->get_motor_policy().test_set_ramp_rate_return_code(
                errors::ErrorCode::MOTOR_ILLEGAL_RAMP_RATE);
            auto message =
                messages::SetAccelerationMessage{.id = 123, .rpm_per_s = 9999};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the motor task should respond with an error") {
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                auto ack = std::get<messages::AcknowledgePrevious>(response);
                REQUIRE(ack.with_error ==
                        errors::ErrorCode::MOTOR_ILLEGAL_RAMP_RATE);
            }
        }
    }
}

SCENARIO("motor task homing", "[motor][homing]") {
    GIVEN("a motor task that is stopped") {
        auto tasks = TaskBuilder::build();
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::STOPPED_UNKNOWN);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(home_message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the motor task should enter homing state") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
            }
        }
    }
    GIVEN("a motor task that is controlling at a slow speed") {
        auto tasks = TaskBuilder::build();
        auto run_message = messages::SetRPMMessage{.id = 123, .target_rpm = 0};
        tasks->get_motor_queue().backing_deque.push_back(
            messages::MotorMessage(run_message));
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::RUNNING);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(home_message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the motor task should enter homing state") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
            }
        }
    }
    GIVEN("a motor task that is controlling at a higher speed") {
        auto tasks = TaskBuilder::build();
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 4500};
        tasks->get_motor_queue().backing_deque.push_back(
            messages::MotorMessage(run_message));
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::RUNNING);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(home_message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the motor task should enter homing state") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
                REQUIRE(
                    std::holds_alternative<messages::CheckHomingStatusMessage>(
                        tasks->get_motor_queue().backing_deque.front()));
                REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
            }
        }
    }

    GIVEN("a motor task that is controlling in the home speed range") {
        auto tasks = TaskBuilder::build();
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 500};
        tasks->get_motor_queue().backing_deque.push_back(
            messages::MotorMessage(run_message));
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::RUNNING);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(home_message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the motor task should enter homing state") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
                REQUIRE(
                    std::holds_alternative<messages::CheckHomingStatusMessage>(
                        tasks->get_motor_queue().backing_deque.front()));
                REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
            }
        }
    }

    GIVEN("a motor task in the moving-to-home-speed state") {
        auto tasks = TaskBuilder::build();
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 500};
        tasks->get_motor_queue().backing_deque.push_back(
            messages::MotorMessage(run_message));
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        tasks->get_motor_queue().backing_deque.push_back(
            messages::BeginHomingMessage{.id = 2213});
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        CHECK(tasks->get_motor_policy().get_target_rpm() >
              std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                  HOMING_ROTATION_LIMIT_LOW_RPM);
        CHECK(tasks->get_motor_policy().get_target_rpm() <
              std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                  HOMING_ROTATION_LIMIT_HIGH_RPM);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
        CHECK(std::holds_alternative<messages::CheckHomingStatusMessage>(
            tasks->get_motor_queue().backing_deque.front()));
        WHEN(
            "checking the homing status while in the appropriate speed range") {
            tasks->get_motor_policy().test_set_current_rpm(
                (std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                     HOMING_ROTATION_LIMIT_HIGH_RPM +
                 std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                     HOMING_ROTATION_LIMIT_LOW_RPM) /
                2);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task goes to coasting and engages the solenoid") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_COASTING_TO_STOP);
                REQUIRE(tasks->get_motor_policy().test_solenoid_engaged());
                REQUIRE(tasks->get_motor_policy().test_solenoid_current() ==
                        std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                            HOMING_SOLENOID_CURRENT_INITIAL);
            }
        }
        WHEN(
            "checking the homing status while not in the appropriate speed "
            "range") {
            tasks->get_motor_policy().test_set_current_rpm(
                std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                    HOMING_ROTATION_LIMIT_HIGH_RPM *
                1.1);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task remains in moving-to-speed and waits for the rpm") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
                REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
                REQUIRE(
                    std::holds_alternative<messages::CheckHomingStatusMessage>(
                        tasks->get_motor_queue().backing_deque.front()));
            }
        }
    }

    GIVEN("a motor task in the wait-for-stop state") {
        auto tasks = TaskBuilder::build();
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 500};
        tasks->get_motor_queue().backing_deque.push_back(
            messages::MotorMessage(run_message));
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        tasks->get_host_comms_queue().backing_deque.clear();
        auto homing_message = messages::BeginHomingMessage{.id = 2213};
        tasks->get_motor_queue().backing_deque.push_back(homing_message);
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        tasks->get_motor_policy().test_set_current_rpm(
            tasks->get_motor_policy().get_target_rpm());
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::HOMING_COASTING_TO_STOP);
        WHEN("receiving an error") {
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorSystemErrorMessage{.errors = 0x2});
            // This must run twice to handle the check-status message from the
            // timeout mechanism before the error message
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task goes to homed state and lowers solenoid current") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::STOPPED_HOMED);
                REQUIRE(tasks->get_motor_policy().test_solenoid_engaged());
                REQUIRE(tasks->get_motor_policy().test_solenoid_current() ==
                        std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                            HOMING_SOLENOID_CURRENT_HOLD);
                REQUIRE(tasks->get_motor_policy().get_target_rpm() == 0);
                auto ack = std::get<messages::AcknowledgePrevious>(
                    tasks->get_host_comms_queue().backing_deque.front());
                REQUIRE(ack.responding_to_id == homing_message.id);
                REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
            }
        }
        WHEN("not receiving an error for too long") {
            for (size_t i = 0;
                 i < std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                         HOMING_CYCLES_BEFORE_TIMEOUT;
                 i++) {
                CHECK(tasks->get_motor_task().get_state() ==
                      motor_task::State::HOMING_COASTING_TO_STOP);
                tasks->get_motor_task().run_once(tasks->get_motor_policy());
                CHECK(tasks->get_motor_policy().test_solenoid_current() ==
                      std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                          HOMING_SOLENOID_CURRENT_INITIAL);
                CHECK(tasks->get_host_comms_queue().backing_deque.empty());
            }
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the home timeout should fire") {
                auto ack_message = std::get<messages::AcknowledgePrevious>(
                    tasks->get_host_comms_queue().backing_deque.front());
                REQUIRE(ack_message.responding_to_id == homing_message.id);
                REQUIRE(ack_message.with_error == errors::ErrorCode::NO_ERROR);
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::STOPPED_HOMED);
            }
        }
    }
}

SCENARIO("motor task debug solenoid handling", "[motor][debug]") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        WHEN("activating the solenoid through the debug mechanism") {
            auto solenoid_message =
                messages::ActuateSolenoidMessage{.id = 123, .current_ma = 500};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(solenoid_message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task actuates the solenoid") {
                REQUIRE(tasks->get_motor_policy().test_solenoid_engaged() ==
                        true);
                REQUIRE(tasks->get_motor_policy().test_solenoid_current() ==
                        solenoid_message.current_ma);
            }
            THEN("the task sends a response") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto ack = std::get<messages::AcknowledgePrevious>(
                    tasks->get_host_comms_queue().backing_deque.front());
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(ack.responding_to_id == solenoid_message.id);
                REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
            }
        }
        WHEN("deactivating the solenoid through the debug mechanism") {
            auto solenoid_message =
                messages::ActuateSolenoidMessage{.id = 221, .current_ma = 0};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(solenoid_message));
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the task deactivates the solenoid") {
                REQUIRE(tasks->get_motor_policy().test_solenoid_engaged() ==
                        false);
            }
            THEN("the task sends a response") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto ack = std::get<messages::AcknowledgePrevious>(
                    tasks->get_host_comms_queue().backing_deque.front());
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(ack.responding_to_id == solenoid_message.id);
                REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
            }
        }
    }
}

SCENARIO("motor task debug plate lock handling", "[motor][debug]") {
    GIVEN("a motor task with the lock off") {
        auto tasks = TaskBuilder::build();
        CHECK(!tasks->get_motor_policy().test_plate_lock_enabled());
        WHEN("activating the plate lock through the debug mechanism") {
            auto lock_message =
                messages::SetPlateLockPowerMessage{.id = 123, .power = 0.5};
            tasks->get_motor_queue().backing_deque.push_back(lock_message);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the lock should be on at the right power") {
                REQUIRE(tasks->get_motor_policy().test_plate_lock_enabled());
                REQUIRE(tasks->get_motor_policy().test_plate_lock_get_power() ==
                        lock_message.power);
            }
            THEN("the message should be acknowledged") {
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .responding_to_id == lock_message.id);
            }
        }
        WHEN("deactivating the plate lock through the debug mechanism") {
            auto lock_message =
                messages::SetPlateLockPowerMessage{.id = 123, .power = 0.0};
            tasks->get_motor_queue().backing_deque.push_back(lock_message);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the lock should still be off") {
                REQUIRE(!tasks->get_motor_policy().test_plate_lock_enabled());
            }
            THEN("the message should be acknowledged") {
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .responding_to_id == lock_message.id);
            }
        }
    }
    GIVEN("a motor task with the lock on") {
        auto tasks = TaskBuilder::build();
        auto lock_message =
            messages::SetPlateLockPowerMessage{.id = 123, .power = 0.5};
        CHECK(!tasks->get_motor_policy().test_plate_lock_enabled());
        tasks->get_motor_queue().backing_deque.push_back(lock_message);
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        CHECK(tasks->get_motor_policy().test_plate_lock_enabled());
        CHECK(tasks->get_motor_policy().test_plate_lock_get_power() ==
              lock_message.power);
        tasks->get_host_comms_queue().backing_deque.clear();
        WHEN(
            "activating the plate lock through the debug mechanism with a "
            "different power") {
            auto relock_message =
                messages::SetPlateLockPowerMessage{.id = 123, .power = -0.5};
            tasks->get_motor_queue().backing_deque.push_back(relock_message);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the lock should be on at the right power") {
                REQUIRE(tasks->get_motor_policy().test_plate_lock_enabled());
                REQUIRE(tasks->get_motor_policy().test_plate_lock_get_power() ==
                        relock_message.power);
            }
            THEN("the message should be acknowledged") {
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .responding_to_id == relock_message.id);
            }
        }
        WHEN("deactivating the plate lock through the debug mechanism") {
            auto unlock_message =
                messages::SetPlateLockPowerMessage{.id = 123, .power = 0.0};
            tasks->get_motor_queue().backing_deque.push_back(unlock_message);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the lock should now be off") {
                REQUIRE(!tasks->get_motor_policy().test_plate_lock_enabled());
            }
            THEN("the message should be acknowledged") {
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .responding_to_id == unlock_message.id);
            }
        }
    }
}
