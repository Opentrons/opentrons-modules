#include "catch2/catch.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/messages.hpp"
#include "test/task_builder.hpp"

SCENARIO("heater task message passing") {
    GIVEN("a heater task with valid temps") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = (1U << 9), .pad_b = (1U << 9), .board = (1U << 9)};
        tasks->get_heater_queue().backing_deque.push_back(
            messages::HeaterMessage(read_message));
        tasks->get_heater_task().run_once();
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
                    REQUIRE(gettemp.current_temperature == 95);
                }
            }
        }
    }
}
