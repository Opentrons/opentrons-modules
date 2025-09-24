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
        WHEN(
            "sending a set-rpm message as if from the host comms and plate "
            "lock not closed") {
            auto message =
                messages::SetRPMMessage{.id = 222, .target_rpm = 1254};
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN(
                    "the task should not set the rpm and disengage solenoid") {
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 0);
                }
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    tasks->require_has_ack_for(
                        message, errors::ErrorCode::PLATE_LOCK_NOT_CLOSED);
                }
                AND_THEN("the task state should still be idle_unknown") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::STOPPED_UNKNOWN);
                }
            }
        }
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        WHEN(
            "sending a set-rpm message as if from the host comms and plate "
            "lock closed") {
            auto message =
                messages::SetRPMMessage{.id = 222, .target_rpm = 1254};
            tasks->get_motor_policy().test_set_current_rpm(50);
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN("the task should set the rpm and disengage solenoid") {
                    REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 1254);
                }
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    tasks->require_has_ack_for(message);
                }
                AND_THEN("the task state should be running") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::RUNNING);
                }
            }
        }
        WHEN(
            "sending a set-rpm message as if from the host comms and motor "
            "hasn't completed homing") {
            tasks->get_motor_policy().test_set_current_rpm(
                50);  // to prevent motor_unable_to_move error
            auto message1 = messages::BeginHomingMessage{.id = 123};
            auto message2 =
                messages::SetRPMMessage{.id = 222, .target_rpm = 1254};
            tasks->consume_motor_message(message1);
            // remove the checkhomingstatus message
            tasks->get_motor_queue().backing_deque.pop_front();
            tasks->consume_motor_message(message2);
            THEN("the task should get the message") {
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    REQUIRE(tasks->get_system_queue().backing_deque.empty());
                    tasks->require_has_ack_for(message2,
                                               errors::ErrorCode::MOTOR_HOMING);
                }
                AND_THEN("the task state should still be idle_unknown") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
                }
            }
        }
        WHEN("sending a set-rpm message as if from the system") {
            tasks->get_motor_policy().test_set_current_rpm(50);
            auto message = messages::SetRPMMessage{
                .id = 222, .target_rpm = 1254, .from_system = true};
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN(
                    "the task should set the rpm and disengage the solenoid") {
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 1254);
                    REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
                }
                AND_THEN(
                    "the task should respond to the message to the system") {
                    REQUIRE_FALSE(
                        tasks->get_system_queue().backing_deque.empty());
                    auto msg = tasks->get_system_queue().backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            msg));
                    auto ack = std::get<messages::AcknowledgePrevious>(msg);
                    REQUIRE(ack.responding_to_id == message.id);
                    REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
                }
                AND_THEN("the task state should be running") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::RUNNING);
                }
            }
        }
        WHEN(
            "sending a set-rpm message as if from the host comms and motor "
            "fails to start") {
            tasks->get_motor_policy().test_set_current_rpm(0);
            auto message =
                messages::SetRPMMessage{.id = 222, .target_rpm = 1254};
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN("the task should disengage solenoid and stop motor") {
                    REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 0);
                }
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    REQUIRE(tasks->get_system_queue().backing_deque.empty());
                    tasks->require_has_ack_for(
                        message, errors::ErrorCode::MOTOR_UNABLE_TO_MOVE);
                }
                AND_THEN("the task state should be error") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::ERROR);
                }
            }
        }
        WHEN(
            "sending a set-rpm message as if from the host comms and motor "
            "fails to start but the motor controller is already in error "
            "state") {
            tasks->get_motor_policy().test_set_current_rpm(0);
            auto message = messages::MotorSystemErrorMessage{
                .errors = static_cast<uint16_t>(
                    1u << errors::MotorErrorOffset::SW_ERROR)};
            tasks->consume_motor_message(message);
            tasks->get_host_comms_queue().backing_deque.pop_front();
            auto message2 =
                messages::SetRPMMessage{.id = 222, .target_rpm = 1254};
            tasks->consume_motor_message(message2);
            THEN("the task should get the message") {
                AND_THEN("the task should disengage solenoid and stop motor") {
                    REQUIRE(!tasks->get_motor_policy().test_solenoid_engaged());
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 0);
                }
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    tasks->require_has_ack_for(
                        message2, errors::ErrorCode::MOTOR_BLDC_DRIVER_ERROR);
                    REQUIRE_FALSE(tasks->get_system_queue()
                                      .backing_deque
                                      .empty());  // for UpdateLEDStateMessage
                }
                AND_THEN("the task state should be error") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::ERROR);
                }
            }
        }
        WHEN("sending a get-rpm message") {
            tasks->get_motor_policy().test_set_current_rpm(1050);
            auto pre_message = messages::SetRPMMessage{
                .id = 123, .target_rpm = 3500};  // needed to populate setpoint
            tasks->consume_motor_message(pre_message);
            tasks->get_host_comms_queue().backing_deque.pop_front();
            auto message = messages::GetRPMMessage{.id = 123};
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN("the task should respond to the message") {
                    auto getrpmresponse =
                        tasks->require_has_ack_for<messages::GetRPMMessage,
                                                   messages::GetRPMResponse>(
                            message);
                    REQUIRE(getrpmresponse.current_rpm == 1050);
                    REQUIRE(getrpmresponse.setpoint_rpm == 3500);
                }
            }
        }

        WHEN("sending a set-acceleration message") {
            auto message =
                messages::SetAccelerationMessage{.id = 123, .rpm_per_s = 9999};
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_policy().test_get_ramp_rate() ==
                        message.rpm_per_s);
                AND_THEN("the task should respond to the message") {
                    tasks->require_has_ack_for(message);
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
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN(
                    "the task should send one error message to host comms and "
                    "system") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.size() ==
                        1);
                    auto error_message = tasks->get_latest_host_comms_message<
                        messages::ErrorMessage>();
                    REQUIRE(error_message.code ==
                            errors::ErrorCode::MOTOR_BLDC_DRIVER_ERROR);
                    REQUIRE(tasks->get_system_queue().backing_deque.size() ==
                            1);
                    auto upstream2 =
                        tasks->get_system_queue().backing_deque.front();
                    tasks->get_system_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::UpdateLEDStateMessage>(
                            upstream2));
                    REQUIRE(std::get<messages::UpdateLEDStateMessage>(upstream2)
                                .color == LED_COLOR::AMBER);
                }
                AND_THEN("the task should enter error state") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::ERROR);
                }
            }
        }
        WHEN("sending a get-rpm message while motor is in error state") {
            auto message = messages::MotorSystemErrorMessage{
                .errors = static_cast<uint16_t>(
                    1u << errors::MotorErrorOffset::SW_ERROR)};
            tasks->consume_motor_message(message);
            tasks->get_host_comms_queue().backing_deque.pop_front();
            auto message2 = messages::GetRPMMessage{.id = 222};
            tasks->consume_motor_message(message2);
            THEN("error response should be received") {
                tasks->require_has_ack_for<messages::GetRPMMessage,
                                           messages::GetRPMResponse>(
                    message2, errors::ErrorCode::MOTOR_BLDC_DRIVER_ERROR);
            }
        }
        WHEN("sending a set-rpm message while motor is in error state") {
            auto message = messages::MotorSystemErrorMessage{
                .errors = static_cast<uint16_t>(
                    1u << errors::MotorErrorOffset::SW_ERROR)};
            tasks->consume_motor_message(message);
            tasks->get_host_comms_queue().backing_deque.pop_front();
            auto message2 =
                messages::SetRPMMessage{.id = 222, .target_rpm = 500};
            tasks->consume_motor_message(message2);
            THEN("error response should be received") {
                tasks->require_has_ack_for(
                    message2, errors::ErrorCode::MOTOR_BLDC_DRIVER_ERROR);
            }
        }
        WHEN("sending an internal error with multiple bits set") {
            auto message = messages::MotorSystemErrorMessage{
                .errors = static_cast<uint16_t>(
                    (1u << errors::MotorErrorOffset::OVERCURRENT) |
                    (1u << errors::MotorErrorOffset::FOC_DURATION) |
                    (1u << errors::MotorErrorOffset::UNDER_VOLT))};
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN(
                    "the task should send one error message for each sent bit "
                    "to host comms") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.size() ==
                        3);
                    auto foc_duration_msg =
                        tasks->get_latest_host_comms_message<
                            messages::ErrorMessage>();
                    REQUIRE(foc_duration_msg.code ==
                            errors::ErrorCode::MOTOR_FOC_DURATION);
                    auto undervolt_msg = tasks->get_latest_host_comms_message<
                        messages::ErrorMessage>();
                    REQUIRE(undervolt_msg.code ==
                            errors::ErrorCode::MOTOR_BLDC_UNDERVOLT);
                    auto overcurrent_msg = tasks->get_latest_host_comms_message<
                        messages::ErrorMessage>();
                    REQUIRE(overcurrent_msg.code ==
                            errors::ErrorCode::MOTOR_BLDC_DRIVER_FAULT);
                }
                AND_THEN("the task should enter error state") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::ERROR);
                }
                AND_WHEN("subsequently queried for its error state") {
                    tasks->get_host_comms_queue().backing_deque.clear();
                    auto query_message =
                        messages::GetErrorStateMessage{.id = 1231};
                    tasks->consume_motor_message(query_message);
                    THEN("the response should carry the error message") {
                        tasks->require_has_ack_for(
                            query_message,
                            errors::ErrorCode::MOTOR_BLDC_DRIVER_FAULT);
                    }
                }
            }
        }

        WHEN("sending an internal error with no bits set") {
            auto message = messages::MotorSystemErrorMessage();
            tasks->consume_motor_message(message);
            THEN("the task should get the message") {
                AND_THEN(
                    "the task should send a spurious-error message "
                    "upstream") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.size() ==
                        1);
                    auto spurious_msg = tasks->get_latest_host_comms_message<
                        messages::ErrorMessage>();
                    REQUIRE(spurious_msg.code ==
                            errors::ErrorCode::MOTOR_SPURIOUS_ERROR);
                }
                AND_THEN("the task should not enter error state") {
                    REQUIRE(tasks->get_motor_task().get_state() ==
                            motor_task::State::STOPPED_UNKNOWN);
                }
                AND_WHEN("subsequently queried for its error state") {
                    tasks->get_host_comms_queue().backing_deque.clear();
                    auto query_message =
                        messages::GetErrorStateMessage{.id = 1231};
                    tasks->consume_motor_message(query_message);
                    THEN("the response should not carry the error message") {
                        tasks->require_has_ack_for(query_message);
                    }
                }
            }
        }
        WHEN("sending a set error status command") {
            auto error_message = messages::SetErrorStateMessage{
                .id = 1231,
                .error_to_set = errors::ErrorCode::MOTOR_BLDC_DRIVER_FAULT,
                .delay_s = 41};
            tasks->consume_motor_message(error_message);
            THEN("the task should respond") {
                tasks->require_has_ack_for(error_message);
            }
            THEN("the task should forward error details to the policy") {
                REQUIRE(
                    tasks->get_motor_policy().test_get_manual_error_was_set());
                REQUIRE(tasks->get_motor_policy().test_get_manual_error() ==
                        (1 << errors::MotorErrorOffset::OVERCURRENT));
                REQUIRE(
                    tasks->get_motor_policy().test_get_manual_error_timeout() ==
                    41);
            }
        }
        WHEN("sending a clear error state command") {
            auto clear_message = messages::ClearErrorStateMessage{.id = 1231};
            tasks->consume_motor_message(clear_message);
            THEN("the task should response") {
                tasks->require_has_ack_for(clear_message);
            }
        }
    }
}

SCENARIO("motor task input error handling", "[motor]") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        WHEN("a command requests an invalid speed") {
            tasks->get_motor_policy().test_set_rpm_return_code(
                errors::ErrorCode::MOTOR_ILLEGAL_SPEED);
            auto message =
                messages::SetRPMMessage{.id = 123, .target_rpm = 9999};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_policy().test_set_current_rpm(50);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("the motor task should respond with an error") {
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::MOTOR_ILLEGAL_SPEED);
            }
        }
        WHEN("a command requests an invalid ramp rate") {
            tasks->get_motor_policy().test_set_ramp_rate_return_code(
                errors::ErrorCode::MOTOR_ILLEGAL_RAMP_RATE);
            auto message =
                messages::SetAccelerationMessage{.id = 123, .rpm_per_s = 9999};
            tasks->consume_motor_message(message);
            THEN("the motor task should respond with an error") {
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::MOTOR_ILLEGAL_RAMP_RATE);
            }
        }
        WHEN(
            "a command requests an invalid speed but the motor controller "
            "is "
            "already in error state") {
            auto message = messages::MotorSystemErrorMessage{
                .errors = static_cast<uint16_t>(
                    1u << errors::MotorErrorOffset::SW_ERROR)};
            tasks->consume_motor_message(message);
            tasks->get_host_comms_queue().backing_deque.pop_front();
            tasks->get_motor_policy().test_set_rpm_return_code(
                errors::ErrorCode::MOTOR_ILLEGAL_SPEED);
            auto message2 =
                messages::SetRPMMessage{.id = 123, .target_rpm = 9999};
            tasks->consume_motor_message(message2);
            tasks->get_motor_policy().test_set_current_rpm(50);
            THEN(
                "the motor task should respond with the a priori motor "
                "controller error") {
                tasks->require_has_ack_for(
                    message2, errors::ErrorCode::MOTOR_BLDC_DRIVER_ERROR);
            }
        }
    }
}

SCENARIO("motor task homing", "[motor][homing]") {
    GIVEN("a motor task that is stopped") {
        auto tasks = TaskBuilder::build();
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::STOPPED_UNKNOWN);
        WHEN("starting a home sequence but motor unable to move") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->consume_motor_message(home_message);
            THEN(
                "the motor task should enter error state and send a "
                "response") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::ERROR);
                REQUIRE(tasks->get_motor_policy().get_target_rpm() == 0);
                tasks->require_has_ack_for(
                    home_message, errors::ErrorCode::MOTOR_UNABLE_TO_MOVE);
            }
        }
    }
    GIVEN("a motor task that is stopped") {
        auto tasks = TaskBuilder::build();
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        WHEN("starting a home sequence with default serial number") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->consume_motor_message(home_message);
            THEN(
                "the motor task should have default homing speed (25 rpm "
                "low)") {
                REQUIRE(tasks->get_motor_task().get_homing_speed() == 200);
            }
        }
        WHEN("starting a home sequence with an old solenoid serial number") {
            std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH> serial_number = {
                "HSV012022113007"};
            tasks->get_motor_policy().set_serial_number(serial_number);
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->consume_motor_message(home_message);
            THEN(
                "the motor task should have default homing speed (25 rpm "
                "low)") {
                REQUIRE(tasks->get_motor_task().get_homing_speed() == 200);
            }
        }
        WHEN("starting a home sequence with a new solenoid serial number") {
            std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH> serial_number = {
                "HSV012022113008"};
            tasks->get_motor_policy().set_serial_number(serial_number);
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->consume_motor_message(home_message);
            THEN(
                "the motor task should have default homing speed (25 rpm "
                "low)") {
                REQUIRE(tasks->get_motor_task().get_homing_speed() == 275);
            }
        }
        WHEN("starting a home sequence with an invalid serial number") {
            std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH> serial_number = {
                "HSV01XX"};
            tasks->get_motor_policy().set_serial_number(serial_number);
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->consume_motor_message(home_message);
            THEN(
                "the motor task should have default homing speed (25 rpm "
                "low)") {
                REQUIRE(tasks->get_motor_task().get_homing_speed() == 200);
            }
        }
    }
    GIVEN("a motor task that is stopped") {
        auto tasks = TaskBuilder::build();
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        tasks->get_host_comms_queue()
            .backing_deque.pop_front();  // clear generated ack message
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::STOPPED_UNKNOWN);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->get_motor_policy().test_set_current_rpm(50);
            tasks->consume_motor_message(home_message);
            THEN("the motor task should enter homing state") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
            }
        }
    }
    GIVEN("a motor task that is controlling at a slow speed") {
        auto tasks = TaskBuilder::build();
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        tasks->get_host_comms_queue()
            .backing_deque.pop_front();  // clear generated ack message
        auto run_message = messages::SetRPMMessage{.id = 123, .target_rpm = 0};
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        tasks->consume_motor_message(run_message);
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::RUNNING);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->get_motor_policy().test_set_current_rpm(50);
            tasks->consume_motor_message(home_message);
            THEN("the motor task should enter homing state") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
            }
        }
    }
    GIVEN("a motor task that is controlling at a higher speed") {
        auto tasks = TaskBuilder::build();
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        tasks->get_host_comms_queue()
            .backing_deque.pop_front();  // clear generated ack message
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 4500};
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        tasks->consume_motor_message(run_message);
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::RUNNING);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->consume_motor_message(home_message);
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
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        tasks->get_host_comms_queue()
            .backing_deque.pop_front();  // clear generated ack message
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 500};
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        tasks->consume_motor_message(run_message);
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::RUNNING);
        WHEN("starting a home sequence") {
            auto home_message = messages::BeginHomingMessage{.id = 123};
            tasks->consume_motor_message(home_message);
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
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        tasks->get_host_comms_queue()
            .backing_deque.pop_front();  // clear generated ack message
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 500};
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        tasks->consume_motor_message(run_message);
        CHECK(tasks->get_motor_policy().get_target_rpm() ==
              run_message.target_rpm);
        tasks->consume_motor_message(messages::BeginHomingMessage{.id = 2213});
        CHECK(tasks->get_motor_policy().get_target_rpm() >
              std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                  HOMING_ROTATION_LIMIT_LOW_OLD_RPM);
        CHECK(tasks->get_motor_policy().get_target_rpm() <
              std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                  HOMING_ROTATION_LIMIT_HIGH_OLD_RPM);
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
        CHECK(std::holds_alternative<messages::CheckHomingStatusMessage>(
            tasks->get_motor_queue().backing_deque.front()));
        WHEN(
            "checking the homing status while in the appropriate speed "
            "range") {
            tasks->get_motor_policy().test_set_current_rpm(
                (std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                     HOMING_ROTATION_LIMIT_HIGH_OLD_RPM +
                 std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                     HOMING_ROTATION_LIMIT_LOW_OLD_RPM) /
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
                    HOMING_ROTATION_LIMIT_HIGH_OLD_RPM *
                1.1);
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN(
                "the task remains in moving-to-speed and waits for the "
                "rpm") {
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
        auto close_pl_message = messages::PlateLockComplete{
            .open = false, .closed = true};  // required before homing
        tasks->consume_motor_message(close_pl_message);
        tasks->get_host_comms_queue()
            .backing_deque.pop_front();  // clear generated ack message
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 500};
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        tasks->consume_motor_message(run_message);
        tasks->get_host_comms_queue().backing_deque.clear();
        auto homing_message = messages::BeginHomingMessage{.id = 2213};
        tasks->consume_motor_message(homing_message);
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
            // This must run twice to handle the check-status message from
            // the timeout mechanism before the error message
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN(
                "the task goes to homed state and lowers solenoid "
                "current") {
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::STOPPED_HOMED);
                REQUIRE(tasks->get_motor_policy().test_solenoid_engaged());
                REQUIRE(tasks->get_motor_policy().test_solenoid_current() ==
                        std::remove_cvref_t<decltype(tasks->get_motor_task())>::
                            HOMING_SOLENOID_CURRENT_HOLD);
                REQUIRE(tasks->get_motor_policy().get_target_rpm() == 0);
                tasks->require_has_ack_for(homing_message);
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
                tasks->require_has_ack_for(homing_message);
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
            tasks->consume_motor_message(solenoid_message);
            THEN("the task actuates the solenoid") {
                REQUIRE(tasks->get_motor_policy().test_solenoid_engaged() ==
                        true);
                REQUIRE(tasks->get_motor_policy().test_solenoid_current() ==
                        solenoid_message.current_ma);
            }
            THEN("the task sends a response") {
                tasks->require_has_ack_for(solenoid_message,
                                           errors::ErrorCode::NO_ERROR);
            }
        }
        WHEN("deactivating the solenoid through the debug mechanism") {
            auto solenoid_message =
                messages::ActuateSolenoidMessage{.id = 221, .current_ma = 0};
            tasks->consume_motor_message(solenoid_message);
            THEN("the task deactivates the solenoid") {
                REQUIRE(tasks->get_motor_policy().test_solenoid_engaged() ==
                        false);
            }
            THEN("the task sends a response") {
                tasks->require_has_ack_for(solenoid_message,
                                           errors::ErrorCode::NO_ERROR);
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
            tasks->consume_motor_message(lock_message);
            THEN("the lock should be on at the right power") {
                REQUIRE(tasks->get_motor_policy().test_plate_lock_enabled());
                REQUIRE(tasks->get_motor_policy().test_plate_lock_get_power() ==
                        lock_message.power);
            }
            THEN("the message should be acknowledged") {
                tasks->require_has_ack_for(lock_message);
            }
        }
        WHEN("deactivating the plate lock through the debug mechanism") {
            auto lock_message =
                messages::SetPlateLockPowerMessage{.id = 123, .power = 0.0};
            tasks->consume_motor_message(lock_message);
            THEN("the lock should still be off") {
                REQUIRE(!tasks->get_motor_policy().test_plate_lock_enabled());
            }
            THEN("the message should be acknowledged") {
                tasks->require_has_ack_for(lock_message);
            }
        }
    }
    GIVEN("a motor task with the lock on") {
        auto tasks = TaskBuilder::build();
        auto lock_message =
            messages::SetPlateLockPowerMessage{.id = 123, .power = 0.5};
        CHECK(!tasks->get_motor_policy().test_plate_lock_enabled());
        tasks->consume_motor_message(lock_message);
        CHECK(tasks->get_motor_policy().test_plate_lock_enabled());
        CHECK(tasks->get_motor_policy().test_plate_lock_get_power() ==
              lock_message.power);
        tasks->get_host_comms_queue().backing_deque.clear();
        WHEN(
            "activating the plate lock through the debug mechanism with a "
            "different power") {
            auto relock_message =
                messages::SetPlateLockPowerMessage{.id = 123, .power = -0.5};
            tasks->consume_motor_message(relock_message);
            THEN("the lock should be on at the right power") {
                REQUIRE(tasks->get_motor_policy().test_plate_lock_enabled());
                REQUIRE(tasks->get_motor_policy().test_plate_lock_get_power() ==
                        relock_message.power);
            }
            THEN("the message should be acknowledged") {
                tasks->require_has_ack_for(relock_message);
            }
        }
        WHEN("deactivating the plate lock through the debug mechanism") {
            auto unlock_message =
                messages::SetPlateLockPowerMessage{.id = 123, .power = 0.0};
            tasks->consume_motor_message(unlock_message);
            THEN("the lock should now be off") {
                REQUIRE(!tasks->get_motor_policy().test_plate_lock_enabled());
            }
            THEN("the message should be acknowledged") {
                tasks->require_has_ack_for(unlock_message);
            }
        }
    }

    GIVEN("a motor task to open plate lock") {
        auto tasks = TaskBuilder::build();
        // first ensure plate lock closed to home
        auto stop_message =
            messages::PlateLockComplete{.open = false, .closed = true};
        tasks->consume_motor_message(stop_message);
        CHECK(!tasks->get_motor_policy().test_plate_lock_enabled());
        CHECK(tasks->get_motor_task().get_plate_lock_state() ==
              motor_task::PlateLockState::IDLE_CLOSED);
        // move state to not homed
        auto run_message =
            messages::SetRPMMessage{.id = 123, .target_rpm = 500};
        tasks->get_motor_policy().test_set_current_rpm(run_message.target_rpm);
        tasks->consume_motor_message(run_message);
        tasks->get_host_comms_queue().backing_deque.clear();
        auto homing_message = messages::BeginHomingMessage{.id = 2213};
        tasks->consume_motor_message(homing_message);
        tasks->get_motor_policy().test_set_current_rpm(
            tasks->get_motor_policy().get_target_rpm());
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::HOMING_MOVING_TO_HOME_SPEED);
        tasks->get_motor_task().run_once(tasks->get_motor_policy());
        CHECK(tasks->get_motor_task().get_state() ==
              motor_task::State::HOMING_COASTING_TO_STOP);
        WHEN("opening the plate lock and not homed") {
            tasks->get_motor_queue().backing_deque.clear();
            auto open_message = messages::OpenPlateLockMessage{.id = 123};
            tasks->get_motor_queue().backing_deque.push_back(open_message);
            // run twice to get to the check-status message
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            tasks->get_motor_task().run_once(tasks->get_motor_policy());
            THEN("an error message should be created") {
                tasks->require_has_ack_for(open_message,
                                           errors::ErrorCode::MOTOR_NOT_HOME);
            }
        }
        WHEN("homing before opening the plate lock") {
            // move state to homed
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
                tasks->require_has_ack_for(homing_message);
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::STOPPED_HOMED);
            }
            tasks->get_host_comms_queue()
                .backing_deque
                .clear();  // clear homing messages in host_comms queue
            WHEN("sending an open plate lock message") {
                auto open_message = messages::OpenPlateLockMessage{.id = 123};
                tasks->consume_motor_message(open_message);
                THEN(
                    "motor should be enabled with correct power and "
                    "state") {
                    REQUIRE(
                        tasks->get_motor_policy().test_plate_lock_enabled());
                    REQUIRE(
                        tasks->get_motor_policy().test_plate_lock_get_power() ==
                        1.0F);
                    REQUIRE(tasks->get_motor_task().get_plate_lock_state() ==
                            motor_task::PlateLockState::OPENING);
                    REQUIRE(std::holds_alternative<
                            messages::CheckPlateLockStatusMessage>(
                        tasks->get_motor_queue().backing_deque.front()));
                    AND_WHEN(
                        "opening plate lock and not receiving a plate "
                        "complete "
                        "event for too long") {
                        for (size_t i = 0;
                             i < std::remove_cvref_t<
                                     decltype(tasks->get_motor_task())>::
                                     PLATE_LOCK_MOVE_TIME_THRESHOLD;
                             i = i + 100) {  // mimic polling_time incrementing
                            CHECK(tasks->get_motor_task()
                                      .get_plate_lock_state() ==
                                  motor_task::PlateLockState::OPENING);
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            CHECK(tasks->get_host_comms_queue()
                                      .backing_deque.empty());
                        }
                        tasks->get_motor_task().run_once(
                            tasks->get_motor_policy());
                        THEN("the plate lock timeout should fire") {
                            REQUIRE(tasks->get_motor_policy()
                                        .test_plate_lock_braked());
                            CHECK(
                                tasks->get_motor_queue().backing_deque.empty());
                            REQUIRE(tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_UNKNOWN);
                            tasks->require_has_ack_for(
                                open_message,
                                errors::ErrorCode::PLATE_LOCK_TIMEOUT);
                        }
                    }
                    tasks->get_motor_queue()
                        .backing_deque
                        .pop_front();  // pulling message out of queue
                    AND_WHEN("a stop condition is sent") {
                        auto stop_message = messages::PlateLockComplete{
                            .open = true, .closed = false};
                        tasks->consume_motor_message(stop_message);
                        THEN("state should update") {
                            REQUIRE(tasks->get_motor_policy()
                                        .test_plate_lock_braked());
                            REQUIRE(tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_OPEN);
                            CHECK(tasks->get_host_comms_queue()
                                      .backing_deque.empty());
                        }
                        AND_WHEN(
                            "check plate lock status message is called") {  // pushing message back in from above
                            auto check_status_message =
                                messages::CheckPlateLockStatusMessage{
                                    .responding_to_id = 234};
                            tasks->consume_motor_message(check_status_message);
                            THEN("nothing should happen") {
                                REQUIRE(tasks->get_motor_policy()
                                            .test_plate_lock_braked());
                                REQUIRE(tasks->get_motor_task()
                                            .get_plate_lock_state() ==
                                        motor_task::PlateLockState::IDLE_OPEN);
                                CHECK(tasks->get_motor_queue()
                                          .backing_deque.empty());
                                tasks->require_has_ack_for_id(234);
                            }
                        }
                        AND_WHEN("another open plate lock message is sent") {
                            auto open_message_2 =
                                messages::OpenPlateLockMessage{.id = 345};
                            tasks->get_motor_queue().backing_deque.push_back(
                                open_message_2);
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            THEN(
                                "plate lock motor shouldn't move and "
                                "acknowledgement should be sent") {
                                CHECK(tasks->get_motor_queue()
                                          .backing_deque.empty());
                                REQUIRE(!tasks->get_motor_policy()
                                             .test_plate_lock_enabled());
                                REQUIRE(tasks->get_motor_policy()
                                            .test_plate_lock_get_power() ==
                                        0.0F);
                                REQUIRE(tasks->get_motor_task()
                                            .get_plate_lock_state() ==
                                        motor_task::PlateLockState::IDLE_OPEN);
                                tasks->require_has_ack_for(open_message_2);
                                REQUIRE(tasks->get_host_comms_queue()
                                            .backing_deque.empty());
                            }
                        }
                    }
                }
            }
        }
        WHEN("homing before closing the plate lock") {
            // move state to homed
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
                tasks->require_has_ack_for(homing_message);
                REQUIRE(tasks->get_motor_task().get_state() ==
                        motor_task::State::STOPPED_HOMED);
            }
            tasks->get_host_comms_queue()
                .backing_deque
                .clear();  // clear homing messages in host_comms queue
            AND_WHEN("sending a regular close plate lock message") {
                auto open_message = messages::OpenPlateLockMessage{
                    .id = 123};  // placing plate lock in not-closed state
                tasks->consume_motor_message(open_message);
                tasks->get_motor_queue()
                    .backing_deque.pop_front();  // pulling out
                                                 // CheckPlateLockStatusMessage
                auto close_message = messages::ClosePlateLockMessage{.id = 123};
                tasks->consume_motor_message(close_message);
                THEN(
                    "motor should be enabled with correct power and "
                    "state") {
                    REQUIRE(
                        tasks->get_motor_policy().test_plate_lock_enabled());
                    REQUIRE(
                        tasks->get_motor_policy().test_plate_lock_get_power() ==
                        -1.0F);
                    REQUIRE(tasks->get_motor_task().get_plate_lock_state() ==
                            motor_task::PlateLockState::CLOSING);
                    REQUIRE(std::holds_alternative<
                            messages::CheckPlateLockStatusMessage>(
                        tasks->get_motor_queue().backing_deque.front()));
                    AND_WHEN(
                        "closing plate lock and not receiving a plate "
                        "complete "
                        "event for too long") {
                        for (size_t i = 0;
                             i < std::remove_cvref_t<
                                     decltype(tasks->get_motor_task())>::
                                     PLATE_LOCK_MOVE_TIME_THRESHOLD;
                             i = i + 100) {  // mimic polling_time incrementing
                            CHECK(tasks->get_motor_task()
                                      .get_plate_lock_state() ==
                                  motor_task::PlateLockState::CLOSING);
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            CHECK(tasks->get_host_comms_queue()
                                      .backing_deque.empty());
                        }
                        tasks->get_motor_task().run_once(
                            tasks->get_motor_policy());
                        THEN("the plate lock timeout should fire") {
                            REQUIRE(tasks->get_motor_policy()
                                        .test_plate_lock_braked());
                            CHECK(
                                tasks->get_motor_queue().backing_deque.empty());
                            REQUIRE(tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_UNKNOWN);
                            auto ack_message =
                                std::get<messages::AcknowledgePrevious>(
                                    tasks->get_host_comms_queue()
                                        .backing_deque.front());
                            REQUIRE(ack_message.responding_to_id ==
                                    close_message.id);
                            REQUIRE(ack_message.with_error ==
                                    errors::ErrorCode::PLATE_LOCK_TIMEOUT);
                        }
                    }
                    tasks->get_motor_queue()
                        .backing_deque
                        .pop_front();  // pulling message out of queue
                    AND_WHEN("a stop condition is sent") {
                        auto stop_message = messages::PlateLockComplete{
                            .open = false, .closed = true};
                        tasks->consume_motor_message(stop_message);
                        THEN(
                            "state should update and send "
                            "acknowledgement") {
                            REQUIRE(tasks->get_motor_policy()
                                        .test_plate_lock_braked());
                            REQUIRE(tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_CLOSED);
                            CHECK(tasks->get_host_comms_queue()
                                      .backing_deque.empty());
                        }
                        AND_WHEN(
                            "check plate lock status message is called") {  // pushing message back in from above
                            auto check_status_message =
                                messages::CheckPlateLockStatusMessage{
                                    .responding_to_id = 234};
                            tasks->consume_motor_message(check_status_message);
                            THEN("nothing should happen") {
                                REQUIRE(tasks->get_motor_policy()
                                            .test_plate_lock_braked());
                                REQUIRE(
                                    tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_CLOSED);
                                CHECK(!tasks->get_host_comms_queue()
                                           .backing_deque.empty());
                                CHECK(tasks->get_motor_queue()
                                          .backing_deque.empty());
                                auto ack_message =
                                    std::get<messages::AcknowledgePrevious>(
                                        tasks->get_host_comms_queue()
                                            .backing_deque.front());
                                REQUIRE(ack_message.responding_to_id ==
                                        check_status_message.responding_to_id);
                                REQUIRE(ack_message.with_error ==
                                        errors::ErrorCode::NO_ERROR);
                            }
                        }
                        AND_WHEN("another close plate lock message is sent") {
                            auto close_message_2 =
                                messages::ClosePlateLockMessage{.id = 234};
                            tasks->get_motor_queue().backing_deque.push_back(
                                close_message_2);
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            THEN(
                                "plate lock motor shouldn't move and "
                                "acknowledgement should be sent") {
                                REQUIRE(tasks->get_motor_queue()
                                            .backing_deque.empty());
                                REQUIRE_FALSE(tasks->get_motor_policy()
                                                  .test_plate_lock_enabled());
                                REQUIRE(tasks->get_motor_policy()
                                            .test_plate_lock_get_power() ==
                                        0.0F);
                                REQUIRE(
                                    tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_CLOSED);
                                tasks->require_has_ack_for(close_message_2);
                                REQUIRE(tasks->get_host_comms_queue()
                                            .backing_deque.empty());
                            }
                        }
                    }
                }
            }
            AND_WHEN("sending a close plate lock message on startup") {
                auto open_message = messages::OpenPlateLockMessage{
                    .id = 123};  // placing plate lock in not-closed state
                tasks->consume_motor_message(open_message);
                tasks->get_motor_queue()
                    .backing_deque.pop_front();  // pulling out
                                                 // CheckPlateLockStatusMessage
                auto close_message = messages::ClosePlateLockMessage{
                    .id = 123, .from_startup = true};
                tasks->consume_motor_message(close_message);
                THEN(
                    "motor should be enabled with correct power and "
                    "state") {
                    REQUIRE(
                        tasks->get_motor_policy().test_plate_lock_enabled());
                    REQUIRE(
                        tasks->get_motor_policy().test_plate_lock_get_power() ==
                        -1.0F);
                    REQUIRE(tasks->get_motor_task().get_plate_lock_state() ==
                            motor_task::PlateLockState::CLOSING);
                    REQUIRE(std::holds_alternative<
                            messages::CheckPlateLockStatusMessage>(
                        tasks->get_motor_queue().backing_deque.front()));
                    AND_WHEN(
                        "closing plate lock and not receiving a plate "
                        "complete "
                        "event for too long") {
                        for (size_t i = 0;
                             i < std::remove_cvref_t<
                                     decltype(tasks->get_motor_task())>::
                                     PLATE_LOCK_MOVE_TIME_THRESHOLD;
                             i = i + 100) {  // mimic polling_time incrementing
                            CHECK(tasks->get_motor_task()
                                      .get_plate_lock_state() ==
                                  motor_task::PlateLockState::CLOSING);
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            CHECK(tasks->get_host_comms_queue()
                                      .backing_deque.empty());
                        }
                        tasks->get_motor_task().run_once(
                            tasks->get_motor_policy());
                        THEN("the plate lock timeout should fire") {
                            REQUIRE(tasks->get_motor_policy()
                                        .test_plate_lock_braked());
                            REQUIRE(tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_UNKNOWN);
                            auto error_message =
                                tasks->get_latest_host_comms_message<
                                    messages::ErrorMessage>();
                            REQUIRE(error_message.code ==
                                    errors::ErrorCode::PLATE_LOCK_TIMEOUT);
                            REQUIRE(tasks->get_host_comms_queue()
                                        .backing_deque.empty());
                        }
                    }
                    auto front_message =
                        tasks->get_motor_queue().backing_deque.front();
                    tasks->get_motor_queue()
                        .backing_deque
                        .pop_front();  // pulling message out of queue
                    AND_WHEN("a stop condition is sent") {
                        auto stop_message = messages::PlateLockComplete{
                            .open = false, .closed = true};
                        tasks->consume_motor_message(stop_message);
                        tasks->consume_motor_message(front_message);
                        auto response =
                            tasks->get_motor_queue().backing_deque.front();
                        tasks->get_motor_queue().backing_deque.pop_front();
                        THEN(
                            "state should update and send "
                            "acknowledgement") {
                            REQUIRE(tasks->get_motor_policy()
                                        .test_plate_lock_braked());
                            REQUIRE(tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_CLOSED);
                            CHECK(tasks->get_host_comms_queue()
                                      .backing_deque.empty());
                            REQUIRE(std::holds_alternative<
                                    messages::BeginHomingMessage>(response));
                        }
                        AND_WHEN(
                            "check plate lock status message is called") {  // pushing message back in from above
                            auto check_status_message =
                                messages::CheckPlateLockStatusMessage{
                                    .responding_to_id = 456};
                            tasks->consume_motor_message(check_status_message);
                            THEN("nothing should happen") {
                                REQUIRE(tasks->get_motor_policy()
                                            .test_plate_lock_braked());
                                REQUIRE(
                                    tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_CLOSED);
                                CHECK(tasks->get_motor_queue()
                                          .backing_deque.empty());
                                tasks->require_has_ack_for_id(456);
                                CHECK(tasks->get_host_comms_queue()
                                          .backing_deque.empty());
                            }
                        }
                        AND_WHEN("another close plate lock message is sent") {
                            auto close_message_2 =
                                messages::ClosePlateLockMessage{.id = 234};
                            tasks->get_motor_queue().backing_deque.push_back(
                                close_message_2);
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            tasks->get_motor_task().run_once(
                                tasks->get_motor_policy());
                            THEN(
                                "plate lock motor shouldn't move and "
                                "acknowledgement should be sent") {
                                CHECK(tasks->get_motor_queue()
                                          .backing_deque.empty());
                                REQUIRE(!tasks->get_motor_policy()
                                             .test_plate_lock_enabled());
                                REQUIRE(tasks->get_motor_policy()
                                            .test_plate_lock_get_power() ==
                                        0.0F);
                                REQUIRE(
                                    tasks->get_motor_task()
                                        .get_plate_lock_state() ==
                                    motor_task::PlateLockState::IDLE_CLOSED);
                                tasks->require_has_ack_for(close_message_2);
                                CHECK(tasks->get_host_comms_queue()
                                          .backing_deque.empty());
                            }
                        }
                    }
                }
            }
        }
    }
}
