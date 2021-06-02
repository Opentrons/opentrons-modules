#include "catch2/catch.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/motor_task.hpp"
#include "test/task_builder.hpp"

SCENARIO("motor task message passing") {
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
                AND_THEN("the task should set the rpm") {
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
                AND_THEN("the task should set the rpm") {
                    REQUIRE(tasks->get_motor_policy().get_target_rpm() == 1254);
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

SCENARIO("motor task error handling") {
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

SCENARIO("motor task input error handling") {
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

SCENARIO("motor task homing") {}
