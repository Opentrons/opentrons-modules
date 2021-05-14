#include "catch2/catch.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/messages.hpp"
#include "test/task_builder.hpp"

SCENARIO("heater task message passing") {
    GIVEN("a heater task with valid temps") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = (1U << 9), .pad_b = (1U << 9), .board = (1U << 11)};
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
                    REQUIRE_THAT(gettemp.current_temperature,
                                 Catch::Matchers::WithinAbs(95.2, .01));
                }
            }
        }

        WHEN("a temperature becomes invalid") {
            read_message = messages::TemperatureConversionComplete{
                .pad_a = (1U << 9), .pad_b = 0, .board = (1U << 11)};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(read_message));
            tasks->get_heater_task().run_once();
            auto error_message =
                tasks->get_host_comms_queue().backing_deque.front();
            tasks->get_host_comms_queue().backing_deque.pop_front();
            CHECK(tasks->get_host_comms_queue().backing_deque.empty());
            THEN("the task should send an error message the first spin") {
                REQUIRE(std::get<messages::ErrorMessage>(error_message).code ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED);
            }
            AND_WHEN("getting the same conversion again") {
                tasks->get_heater_queue().backing_deque.push_back(
                    messages::HeaterMessage(read_message));
                tasks->get_heater_task().run_once();
                THEN("the task should not send another error message") {
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.empty());
                }
            }
            AND_WHEN("sending get-temp messages") {
                auto message = messages::GetTemperatureMessage{.id = 999};
                tasks->get_heater_queue().backing_deque.push_back(
                    messages::HeaterMessage(message));
                tasks->get_heater_queue().backing_deque.push_back(
                    messages::HeaterMessage(message));
                tasks->get_heater_task().run_once();
                tasks->get_heater_task().run_once();
                THEN("the task should return an error both times") {
                    auto first =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    auto second =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    CHECK(tasks->get_host_comms_queue().backing_deque.empty());
                    REQUIRE(
                        std::get<messages::GetTemperatureResponse>(first)
                            .with_error ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED);
                    REQUIRE(
                        std::get<messages::GetTemperatureResponse>(second)
                            .with_error ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED);
                }
            }
            AND_WHEN(
                "getting a subsequent conversion with valid temperatures") {
                read_message.pad_b = (1U << 9);
                tasks->get_heater_queue().backing_deque.push_back(
                    messages::HeaterMessage(read_message));
                tasks->get_heater_task().run_once();
                CHECK(tasks->get_host_comms_queue().backing_deque.empty());
                THEN("get-temp messages should return ok") {
                    auto message = messages::GetTemperatureMessage{.id = 999};
                    tasks->get_heater_queue().backing_deque.push_back(
                        messages::HeaterMessage(message));
                    tasks->get_heater_task().run_once();
                    auto resp =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(std::get<messages::GetTemperatureResponse>(resp)
                                .with_error == errors::ErrorCode::NO_ERROR);
                }
            }
        }
    }

    GIVEN("a heater task with an invalid out-of-range temp") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = (1U << 9), .pad_b = 0, .board = (1U << 11)};
        tasks->get_heater_queue().backing_deque.push_back(
            messages::HeaterMessage(read_message));
        tasks->get_heater_task().run_once();
        CHECK(std::holds_alternative<messages::ErrorMessage>(
            tasks->get_host_comms_queue().backing_deque.front()));
        tasks->get_host_comms_queue().backing_deque.pop_front();
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
        WHEN("sending a set-temperature message") {
            auto message = messages::SetTemperatureMessage{
                .id = 1231, .target_temperature = 60};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->get_heater_task().run_once();
            THEN("the task should respond with an error") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto ack = std::get<messages::AcknowledgePrevious>(response);
                REQUIRE(ack.responding_to_id == message.id);
                REQUIRE(ack.with_error ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED);
            }
        }
        WHEN("sending a get-temperature message") {
            auto message = messages::GetTemperatureMessage{.id = 2222};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->get_heater_task().run_once();
            THEN("the task should respond with an error") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto ack = std::get<messages::GetTemperatureResponse>(response);
                REQUIRE(ack.responding_to_id == message.id);
                REQUIRE(ack.with_error ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED);
            }
        }
    }
}
