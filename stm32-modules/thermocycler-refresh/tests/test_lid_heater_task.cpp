#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/lid_heater_task.hpp"
#include "thermocycler-refresh/messages.hpp"

constexpr int _valid_adc = 6360;  // Gives 50C
constexpr double _valid_temp = 50.0;
constexpr int _shorted_adc = 0;
constexpr int _disconnected_adc = 0x5DC0;

SCENARIO("lid heater task message passing") {
    GIVEN("a lid heater task with valid temps") {
        auto tasks = TaskBuilder::build();
        auto read_message =
            messages::LidTempReadComplete{.lid_temp = _valid_adc};
        tasks->get_lid_heater_queue().backing_deque.push_back(
            messages::LidHeaterMessage(read_message));
        tasks->run_lid_heater_task();

        WHEN("sending a get-lid-temperature-debug message") {
            auto message = messages::GetLidTemperatureDebugMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the messsage") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<
                            messages::GetLidTemperatureDebugResponse>(
                        response));
                    auto gettemp =
                        std::get<messages::GetLidTemperatureDebugResponse>(
                            response);
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE_THAT(gettemp.lid_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.lid_adc == _valid_adc);
                }
            }
        }
        WHEN("sending a get-lid-temperature message") {
            auto message = messages::GetLidTempMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the messsage") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<
                            messages::GetLidTempResponse>(
                        response));
                    auto gettemp =
                        std::get<messages::GetLidTempResponse>(
                            response);
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE_THAT(gettemp.current_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE_THAT(gettemp.set_temp,
                                 Catch::Matchers::WithinAbs(0.0, 0.1));
                }
            }
        }
        WHEN("Sending a SetHeaterDebug message to enable the heater") {
            auto message =
                messages::SetHeaterDebugMessage{.id = 123, .power = 0.65};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should act on the message") {
                    REQUIRE(
                        tasks->get_thermal_plate_queue().backing_deque.empty());

                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto response_msg =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(response_msg.responding_to_id == 123);
                    REQUIRE(response_msg.with_error ==
                            errors::ErrorCode::NO_ERROR);
                    REQUIRE(tasks->get_lid_heater_policy().get_heater_power() ==
                            0.65);
                }
            }
        }
    }
    GIVEN("a heater task with a shorted temp") {
        auto tasks = TaskBuilder::build();
        auto read_message =
            messages::LidTempReadComplete{.lid_temp = _shorted_adc};
        tasks->get_lid_heater_queue().backing_deque.push_back(
            messages::LidHeaterMessage(read_message));
        tasks->run_lid_heater_task();
        CHECK(std::holds_alternative<messages::ErrorMessage>(
            tasks->get_host_comms_queue().backing_deque.front()));
        auto error_msg = std::get<messages::ErrorMessage>(
            tasks->get_host_comms_queue().backing_deque.front());
        CHECK(error_msg.code == errors::ErrorCode::THERMISTOR_LID_SHORT);
        tasks->get_host_comms_queue().backing_deque.pop_front();
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());

        WHEN("Sending a SetHeaterDebug message to enable the heater") {
            auto message =
                messages::SetHeaterDebugMessage{.id = 124, .power = 0.65};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond with an error") {
                    REQUIRE(
                        tasks->get_lid_heater_queue().backing_deque.empty());

                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto response_msg =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(response_msg.responding_to_id == 124);
                    REQUIRE(response_msg.with_error !=
                            errors::ErrorCode::NO_ERROR);
                    REQUIRE(tasks->get_lid_heater_policy().get_heater_power() ==
                            0.0F);
                }
            }
        }
        WHEN("sending a get-lid-temperature message") {
            auto message = messages::GetLidTempMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond with a temp of 0") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<
                            messages::GetLidTempResponse>(
                        response));
                    auto gettemp =
                        std::get<messages::GetLidTempResponse>(
                            response);
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE_THAT(gettemp.current_temp,
                                 Catch::Matchers::WithinAbs(0.0, 0.1));
                    REQUIRE_THAT(gettemp.set_temp,
                                 Catch::Matchers::WithinAbs(0.0, 0.1));
                }
            }
        }
    }
    GIVEN("a heater task with a disconnected thermistor") {
        auto tasks = TaskBuilder::build();
        auto read_message =
            messages::LidTempReadComplete{.lid_temp = _disconnected_adc};
        tasks->get_lid_heater_queue().backing_deque.push_back(
            messages::LidHeaterMessage(read_message));
        tasks->run_lid_heater_task();
        CHECK(std::holds_alternative<messages::ErrorMessage>(
            tasks->get_host_comms_queue().backing_deque.front()));
        auto error_msg = std::get<messages::ErrorMessage>(
            tasks->get_host_comms_queue().backing_deque.front());
        CHECK(error_msg.code == errors::ErrorCode::THERMISTOR_LID_DISCONNECTED);
        tasks->get_host_comms_queue().backing_deque.pop_front();
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());

        WHEN("Sending a SetHeaterDebug message to enable the heater") {
            auto message =
                messages::SetHeaterDebugMessage{.id = 124, .power = 0.65};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond with an error") {
                    REQUIRE(
                        tasks->get_lid_heater_queue().backing_deque.empty());

                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto response_msg =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(response_msg.responding_to_id == 124);
                    REQUIRE(response_msg.with_error !=
                            errors::ErrorCode::NO_ERROR);
                    REQUIRE(tasks->get_lid_heater_policy().get_heater_power() ==
                            0.0F);
                }
            }
        }
    }
}
