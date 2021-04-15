#include "catch2/catch.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/motor_task.hpp"
#include "test/task_builder.hpp"

SCENARIO("motor task basic functionality") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        WHEN("calling run_once() with no messages") {
            THEN("the task should wokr fine") {
                REQUIRE_NOTHROW(tasks->get_heater_task().run_once());
            }
        }
    }
}

SCENARIO("motor task message poassing") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        WHEN("sending a set-rpm message") {
            auto message =
                messages::SetRPMMessage{.id = 222, .target_rpm = 1254};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_task().run_once();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_motor_queue().backing_deque.empty());
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
        WHEN("sending a get-rpm message") {
            auto message = messages::GetRPMMessage{.id = 123};
            tasks->get_motor_queue().backing_deque.push_back(
                messages::MotorMessage(message));
            tasks->get_motor_task().run_once();
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
    }
}
