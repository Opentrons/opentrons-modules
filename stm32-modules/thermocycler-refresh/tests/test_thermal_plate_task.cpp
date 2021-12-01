#include <iterator>
#include <list>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/thermal_plate_task.hpp"

constexpr int _valid_adc = 6360;  // Gives 50C
constexpr double _valid_temp = 50.0;
constexpr int _shorted_adc = 0;
constexpr int _disconnected_adc = 0x5DC0;

using ErrorList = std::list<errors::ErrorCode>;

SCENARIO("thermal plate task message passing") {
    GIVEN("a thermal plate task with valid temps") {
        auto tasks = TaskBuilder::build();
        auto read_message =
            messages::ThermalPlateTempReadComplete{.heat_sink = _valid_adc,
                                                   .front_right = _valid_adc,
                                                   .front_center = _valid_adc,
                                                   .front_left = _valid_adc,
                                                   .back_right = _valid_adc,
                                                   .back_center = _valid_adc,
                                                   .back_left = _valid_adc};
        tasks->get_thermal_plate_queue().backing_deque.push_back(
            messages::ThermalPlateMessage(read_message));
        tasks->run_thermal_plate_task();

        REQUIRE(!tasks->get_thermal_plate_policy()._enabled);

        WHEN("sending a get-lid-temperature-debug message") {
            auto message = messages::GetPlateTemperatureDebugMessage{.id = 123};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());
                AND_THEN("the task should respond to the messsage") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<
                            messages::GetPlateTemperatureDebugResponse>(
                        response));
                    auto gettemp =
                        std::get<messages::GetPlateTemperatureDebugResponse>(
                            response);

                    REQUIRE(gettemp.responding_to_id == message.id);

                    REQUIRE_THAT(gettemp.heat_sink_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.heat_sink_adc == _valid_adc);

                    REQUIRE_THAT(gettemp.front_right_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.front_right_adc == _valid_adc);

                    REQUIRE_THAT(gettemp.front_center_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.front_center_adc == _valid_adc);

                    REQUIRE_THAT(gettemp.front_left_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.front_left_adc == _valid_adc);

                    REQUIRE_THAT(gettemp.back_right_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.back_right_adc == _valid_adc);

                    REQUIRE_THAT(gettemp.back_center_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.back_center_adc == _valid_adc);

                    REQUIRE_THAT(gettemp.back_left_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.back_left_adc == _valid_adc);
                }
            }
        }
        WHEN("sending a SetPeltierDebug message to turn on all peltiers") {
            auto message = messages::SetPeltierDebugMessage{
                .id = 123,
                .power = 0.5F,
                .direction = PeltierDirection::PELTIER_COOLING,
                .selection = PeltierSelection::ALL};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());

                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    response));
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .responding_to_id == 123);
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .with_error == errors::ErrorCode::NO_ERROR);
                auto policy = tasks->get_thermal_plate_policy();
                REQUIRE(policy._enabled);
                REQUIRE(policy._left.power == 0.5F);
                REQUIRE(policy._left.direction ==
                        PeltierDirection::PELTIER_COOLING);
                REQUIRE(policy._right.power == 0.5F);
                REQUIRE(policy._right.direction ==
                        PeltierDirection::PELTIER_COOLING);
                REQUIRE(policy._center.power == 0.5F);
                REQUIRE(policy._center.direction ==
                        PeltierDirection::PELTIER_COOLING);

                WHEN(
                    "sending a SetPeltierDebug message to disable one of the "
                    "peltiers") {
                    message = messages::SetPeltierDebugMessage{
                        .id = 124,
                        .power = 0.0F,
                        .direction = PeltierDirection::PELTIER_HEATING,
                        .selection = PeltierSelection::LEFT};
                    tasks->get_thermal_plate_queue().backing_deque.push_back(
                        messages::ThermalPlateMessage(message));
                    tasks->run_thermal_plate_task();
                    THEN("the other peltiers are still enabled") {
                        policy = tasks->get_thermal_plate_policy();
                        REQUIRE(policy._enabled);
                        REQUIRE(policy._left.power == 0.0F);
                        REQUIRE(policy._left.direction ==
                                PeltierDirection::PELTIER_HEATING);
                        REQUIRE(policy._right.power == 0.5F);
                        REQUIRE(policy._right.direction ==
                                PeltierDirection::PELTIER_COOLING);
                        REQUIRE(policy._center.power == 0.5F);
                        REQUIRE(policy._center.direction ==
                                PeltierDirection::PELTIER_COOLING);
                    }
                }

                WHEN(
                    "sending a SetPeltierDebug message to disable all of the "
                    "peltiers") {
                    message = messages::SetPeltierDebugMessage{
                        .id = 124,
                        .power = 0.0F,
                        .direction = PeltierDirection::PELTIER_HEATING,
                        .selection = PeltierSelection::ALL};
                    tasks->get_thermal_plate_queue().backing_deque.push_back(
                        messages::ThermalPlateMessage(message));
                    tasks->run_thermal_plate_task();
                    THEN("all peltiers are disabled") {
                        policy = tasks->get_thermal_plate_policy();
                        REQUIRE(!policy._enabled);
                        REQUIRE(policy._left.power == 0.0F);
                        REQUIRE(policy._left.direction ==
                                PeltierDirection::PELTIER_HEATING);
                        REQUIRE(policy._right.power == 0.0F);
                        REQUIRE(policy._right.direction ==
                                PeltierDirection::PELTIER_HEATING);
                        REQUIRE(policy._center.power == 0.0F);
                        REQUIRE(policy._center.direction ==
                                PeltierDirection::PELTIER_HEATING);
                    }
                }
            }
        }
        WHEN("sending a SetFanManual message to turn on the fan") {
            auto message =
                messages::SetFanManualMessage{.id = 123, .power = 0.5};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());
                AND_THEN("the task should act on the message") {
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
                    REQUIRE(tasks->get_thermal_plate_policy()._fan_power ==
                            0.5);
                }
            }
        }
    }
    GIVEN("a thermal plate task with shorted thermistors") {
        auto tasks = TaskBuilder::build();
        auto read_message =
            messages::ThermalPlateTempReadComplete{.heat_sink = _shorted_adc,
                                                   .front_right = _shorted_adc,
                                                   .front_center = _shorted_adc,
                                                   .front_left = _shorted_adc,
                                                   .back_right = _shorted_adc,
                                                   .back_center = _shorted_adc,
                                                   .back_left = _shorted_adc};
        // Order of errors doesn't care, so we use a list
        ErrorList errors = {
            errors::ErrorCode::THERMISTOR_HEATSINK_SHORT,
            errors::ErrorCode::THERMISTOR_FRONT_RIGHT_SHORT,
            errors::ErrorCode::THERMISTOR_FRONT_LEFT_SHORT,
            errors::ErrorCode::THERMISTOR_FRONT_CENTER_SHORT,
            errors::ErrorCode::THERMISTOR_BACK_RIGHT_SHORT,
            errors::ErrorCode::THERMISTOR_BACK_LEFT_SHORT,
            errors::ErrorCode::THERMISTOR_BACK_CENTER_SHORT,
        };
        tasks->get_thermal_plate_queue().backing_deque.push_back(
            messages::ThermalPlateMessage(read_message));
        tasks->run_thermal_plate_task();
        // Check that each error is reported
        while (!errors.empty()) {
            CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
            CHECK(std::holds_alternative<messages::ErrorMessage>(
                tasks->get_host_comms_queue().backing_deque.front()));
            auto error_msg = std::get<messages::ErrorMessage>(
                tasks->get_host_comms_queue().backing_deque.front());
            auto error_itr =
                std::find(std::begin(errors), std::end(errors), error_msg.code);
            CHECK(error_itr != std::end(errors));
            tasks->get_host_comms_queue().backing_deque.pop_front();
            errors.erase(error_itr);
        }
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());

        WHEN("trying to send a SetPeltierDebug message to turn on peltiers") {
            auto message = messages::SetPeltierDebugMessage{
                .id = 123,
                .power = 0.5F,
                .direction = PeltierDirection::PELTIER_COOLING,
                .selection = PeltierSelection::ALL};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should respond with an error") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());

                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    response));
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .responding_to_id == 123);
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .with_error != errors::ErrorCode::NO_ERROR);
            }
        }
        WHEN("sending a SetFanManual message to turn on the fan") {
            auto message =
                messages::SetFanManualMessage{.id = 123, .power = 0.5};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());
                AND_THEN("the task should respond with an error") {
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
                    REQUIRE(response_msg.with_error !=
                            errors::ErrorCode::NO_ERROR);
                    REQUIRE(tasks->get_thermal_plate_policy()._fan_power ==
                            0.0);
                }
            }
        }
    }
    GIVEN("a thermal plate task with disconnected thermistors") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::ThermalPlateTempReadComplete{
            .heat_sink = _disconnected_adc,
            .front_right = _disconnected_adc,
            .front_center = _disconnected_adc,
            .front_left = _disconnected_adc,
            .back_right = _disconnected_adc,
            .back_center = _disconnected_adc,
            .back_left = _disconnected_adc};
        // Order of errors doesn't care, so we use a list
        ErrorList errors = {
            errors::ErrorCode::THERMISTOR_HEATSINK_DISCONNECTED,
            errors::ErrorCode::THERMISTOR_FRONT_RIGHT_DISCONNECTED,
            errors::ErrorCode::THERMISTOR_FRONT_LEFT_DISCONNECTED,
            errors::ErrorCode::THERMISTOR_FRONT_CENTER_DISCONNECTED,
            errors::ErrorCode::THERMISTOR_BACK_RIGHT_DISCONNECTED,
            errors::ErrorCode::THERMISTOR_BACK_LEFT_DISCONNECTED,
            errors::ErrorCode::THERMISTOR_BACK_CENTER_DISCONNECTED,
        };
        tasks->get_thermal_plate_queue().backing_deque.push_back(
            messages::ThermalPlateMessage(read_message));
        tasks->run_thermal_plate_task();
        // Check that each error is reported
        while (!errors.empty()) {
            CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
            CHECK(std::holds_alternative<messages::ErrorMessage>(
                tasks->get_host_comms_queue().backing_deque.front()));
            auto error_msg = std::get<messages::ErrorMessage>(
                tasks->get_host_comms_queue().backing_deque.front());
            auto error_itr =
                std::find(std::begin(errors), std::end(errors), error_msg.code);
            CHECK(error_itr != std::end(errors));
            tasks->get_host_comms_queue().backing_deque.pop_front();
            errors.erase(error_itr);
        }
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
    }
}
