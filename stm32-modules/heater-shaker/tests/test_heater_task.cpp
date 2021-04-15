#include "catch2/catch.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/messages.hpp"
#include "test/task_builder.hpp"

SCENARIO("heater task basic functionality") {
    GIVEN("a heater task") {
        auto tasks = TaskBuilder::build();
        WHEN("calling run_once() with no messages") {
            THEN("the task should work fine") {
                REQUIRE_NOTHROW(tasks->get_heater_task().run_once());
            }
        }
    }
}

SCENARIO("heater task message passing") {
    GIVEN("a heater task") {
        auto tasks = TaskBuilder::build();
        WHEN("sending a set-temperature message") {
            auto message = messages::SetTemperatureMessage{
                .id = 1231, .target_temperature = 45};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->get_heater_task().run_once();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
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
        WHEN("sending a get-temperature message") {
            auto message = messages::GetTemperatureMessage{.id = 999};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->get_heater_task().run_once();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the message") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<
                            messages::GetTemperatureResponse>(response));
                    auto gettemp =
                        std::get<messages::GetTemperatureResponse>(response);
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE(gettemp.setpoint_temperature == 48);
                    REQUIRE(gettemp.current_temperature == 99);
                }
            }
        }
    }
}
