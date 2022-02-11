#include <iterator>
#include <list>

#include "catch2/catch.hpp"
#include "core/thermistor_conversion.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/thermal_plate_task.hpp"

constexpr double _valid_temp = 25.0;
constexpr int _shorted_adc = 0;
constexpr int _disconnected_adc = 0x5DC0;

static auto _converter = thermistor_conversion::Conversion<lookups::KS103J2G>(
    thermal_plate_task::ThermalPlateTask<
        TestMessageQueue>::THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
    thermal_plate_task::ThermalPlateTask<TestMessageQueue>::ADC_BIT_MAX, false);

using ErrorList = std::list<errors::ErrorCode>;

SCENARIO("thermal plate task message passing") {
    GIVEN("a thermal plate task with valid temps") {
        auto tasks = TaskBuilder::build();
        auto valid_adc = _converter.backconvert(_valid_temp);
        auto read_message =
            messages::ThermalPlateTempReadComplete{.heat_sink = valid_adc,
                                                   .front_right = valid_adc,
                                                   .front_center = valid_adc,
                                                   .front_left = valid_adc,
                                                   .back_right = valid_adc,
                                                   .back_center = valid_adc,
                                                   .back_left = valid_adc};
        tasks->get_thermal_plate_queue().backing_deque.push_back(
            messages::ThermalPlateMessage(read_message));
        tasks->run_thermal_plate_task();

        REQUIRE(!tasks->get_thermal_plate_policy()._enabled);

        WHEN("sending a get-plate-temperature-debug message") {
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
                    REQUIRE(gettemp.heat_sink_adc == valid_adc);

                    REQUIRE_THAT(gettemp.front_right_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.front_right_adc == valid_adc);

                    REQUIRE_THAT(gettemp.front_center_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.front_center_adc == valid_adc);

                    REQUIRE_THAT(gettemp.front_left_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.front_left_adc == valid_adc);

                    REQUIRE_THAT(gettemp.back_right_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.back_right_adc == valid_adc);

                    REQUIRE_THAT(gettemp.back_center_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.back_center_adc == valid_adc);

                    REQUIRE_THAT(gettemp.back_left_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE(gettemp.back_left_adc == valid_adc);
                }
            }
            THEN("the fan should be disabled") {
                REQUIRE(tasks->get_thermal_plate_policy()._fan_power == 0.0F);
                AND_WHEN("the heatsink is an unsafe temperature") {
                    read_message.heat_sink = _converter.backconvert(80.0F);
                    tasks->get_thermal_plate_queue().backing_deque.push_back(
                        messages::ThermalPlateMessage(read_message));
                    tasks->run_thermal_plate_task();
                    REQUIRE(
                        tasks->get_thermal_plate_queue().backing_deque.empty());
                    THEN("the fan should be at 0.8 power") {
                        REQUIRE_THAT(
                            tasks->get_thermal_plate_policy()._fan_power,
                            Catch::Matchers::WithinAbs(0.8, 0.01));
                    }
                }
            }
        }
        WHEN("sending a get-plate-temperature message") {
            auto message = messages::GetPlateTempMessage{.id = 123};
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
                    REQUIRE(
                        std::holds_alternative<messages::GetPlateTempResponse>(
                            response));
                    auto gettemp =
                        std::get<messages::GetPlateTempResponse>(response);
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE_THAT(gettemp.current_temp,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                    REQUIRE_THAT(gettemp.set_temp,
                                 Catch::Matchers::WithinAbs(0.0, 0.1));
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
            AND_WHEN("sending a SetFanAutomatic to turn the fans off") {
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto set_fan_auto = messages::SetFanAutomaticMessage{.id = 555};
                tasks->get_thermal_plate_queue().backing_deque.push_back(
                    messages::ThermalPlateMessage(set_fan_auto));
                tasks->run_thermal_plate_task();
                THEN("the task should get the message") {
                    REQUIRE(
                        tasks->get_thermal_plate_queue().backing_deque.empty());
                    AND_THEN("the task should act on the message") {
                        REQUIRE(!tasks->get_host_comms_queue()
                                     .backing_deque.empty());
                        auto response =
                            tasks->get_host_comms_queue().backing_deque.front();
                        tasks->get_host_comms_queue().backing_deque.pop_front();
                        REQUIRE(std::holds_alternative<
                                messages::AcknowledgePrevious>(response));
                        auto response_msg =
                            std::get<messages::AcknowledgePrevious>(response);
                        REQUIRE(response_msg.responding_to_id == 555);
                        REQUIRE(response_msg.with_error ==
                                errors::ErrorCode::NO_ERROR);
                        REQUIRE(tasks->get_thermal_plate_policy()._fan_power ==
                                0.0F);
                    }
                }
            }
        }
        WHEN("Sending a SetPIDConstants to configure the plate constants") {
            auto message = messages::SetPIDConstantsMessage{
                .id = 123,
                .selection = PidSelection::PELTIERS,
                .p = 1,
                .i = 1,
                .d = 1};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());
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
                }
            }
        }
        WHEN("Sending a SetPIDConstants with invalid constants") {
            auto message = messages::SetPIDConstantsMessage{
                .id = 555,
                .selection = PidSelection::PELTIERS,
                .p = 1000,
                .i = 1,
                .d = 1};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());
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
                    REQUIRE(response_msg.responding_to_id == 555);
                    REQUIRE(response_msg.with_error ==
                            errors::ErrorCode::THERMAL_CONSTANT_OUT_OF_RANGE);
                }
            }
        }
        WHEN("Sending a SetPlateTemperature message to enable the plate") {
            auto message = messages::SetPlateTemperatureMessage{
                .id = 123, .setpoint = 90.0F, .hold_time = 10.0F};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());
                AND_THEN("the task should respond to the message") {
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
                    AND_WHEN("sending a GetPlateTemp query") {
                        auto tempMessage =
                            messages::GetPlateTempMessage{.id = 555};
                        tasks->get_thermal_plate_queue()
                            .backing_deque.push_back(
                                messages::ThermalPlateMessage(tempMessage));
                        tasks->run_thermal_plate_task();
                        THEN("the response should have the new setpoint") {
                            REQUIRE(!tasks->get_host_comms_queue()
                                         .backing_deque.empty());
                            REQUIRE(std::get<messages::GetPlateTempResponse>(
                                        tasks->get_host_comms_queue()
                                            .backing_deque.front())
                                        .set_temp == message.setpoint);
                        }
                    }
                }
                AND_WHEN("sending updated temperatures below target") {
                    tasks->get_thermal_plate_queue().backing_deque.push_back(
                        messages::ThermalPlateMessage(read_message));
                    tasks->run_thermal_plate_task();
                    THEN("the peltiers should be enabled") {
                        auto p_right =
                            tasks->get_thermal_plate_policy().get_peltier(
                                PeltierID::PELTIER_RIGHT);
                        REQUIRE(p_right.first ==
                                PeltierDirection::PELTIER_HEATING);
                        REQUIRE(p_right.second > 0.0F);
                        auto p_left =
                            tasks->get_thermal_plate_policy().get_peltier(
                                PeltierID::PELTIER_LEFT);
                        REQUIRE(p_left.first ==
                                PeltierDirection::PELTIER_HEATING);
                        REQUIRE(p_left.second > 0.0F);
                        auto p_center =
                            tasks->get_thermal_plate_policy().get_peltier(
                                PeltierID::PELTIER_CENTER);
                        REQUIRE(p_center.first ==
                                PeltierDirection::PELTIER_HEATING);
                        REQUIRE(p_center.second > 0.0F);
                    }
                }
            }
            AND_WHEN("sending a DeactivatePlate command") {
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto tempMessage = messages::DeactivatePlateMessage{.id = 321};
                tasks->get_thermal_plate_queue().backing_deque.push_back(
                    messages::ThermalPlateMessage(tempMessage));
                tasks->run_thermal_plate_task();
                THEN("the task should respond to the message") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    REQUIRE(
                        std::get<messages::AcknowledgePrevious>(
                            tasks->get_host_comms_queue().backing_deque.front())
                            .responding_to_id == 321);
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    AND_WHEN("sending a GetPlateTemp query") {
                        auto tempMessage =
                            messages::GetPlateTempMessage{.id = 555};
                        tasks->get_thermal_plate_queue()
                            .backing_deque.push_back(
                                messages::ThermalPlateMessage(tempMessage));
                        tasks->run_thermal_plate_task();
                        THEN("the response should have no setpoint") {
                            REQUIRE(!tasks->get_host_comms_queue()
                                         .backing_deque.empty());
                            REQUIRE(std::get<messages::GetPlateTempResponse>(
                                        tasks->get_host_comms_queue()
                                            .backing_deque.front())
                                        .set_temp == 0.0F);
                        }
                    }
                }
            }
            AND_WHEN(
                "Sending a SetPIDConstants to configure the peltier "
                "constants") {
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto message = messages::SetPIDConstantsMessage{
                    .id = 808,
                    .selection = PidSelection::PELTIERS,
                    .p = 1,
                    .i = 1,
                    .d = 1};
                tasks->get_thermal_plate_queue().backing_deque.push_back(
                    messages::ThermalPlateMessage(message));
                tasks->run_thermal_plate_task();
                THEN("the task should get the message") {
                    REQUIRE(
                        tasks->get_thermal_plate_queue().backing_deque.empty());
                    AND_THEN("the task should respond with a busy error") {
                        REQUIRE(tasks->get_thermal_plate_queue()
                                    .backing_deque.empty());

                        REQUIRE(!tasks->get_host_comms_queue()
                                     .backing_deque.empty());
                        auto response =
                            tasks->get_host_comms_queue().backing_deque.front();
                        tasks->get_host_comms_queue().backing_deque.pop_front();
                        REQUIRE(std::holds_alternative<
                                messages::AcknowledgePrevious>(response));
                        auto response_msg =
                            std::get<messages::AcknowledgePrevious>(response);
                        REQUIRE(response_msg.responding_to_id == 808);
                        REQUIRE(response_msg.with_error ==
                                errors::ErrorCode::THERMAL_PLATE_BUSY);
                    }
                }
            }
        }
        GIVEN("some power on the peltiers and fans") {
            auto &policy = tasks->get_thermal_plate_policy();
            policy._left.power = 0.1;
            policy._center.power = 0.2;
            policy._center.direction = PeltierDirection::PELTIER_COOLING;
            policy._right.power = 0.3;
            policy._fan_power = 1.0;
            WHEN("sending GetThermalPowerMessage") {
                auto message = messages::GetThermalPowerMessage{.id = 123};
                tasks->get_thermal_plate_queue().backing_deque.push_back(
                    message);
                tasks->run_thermal_plate_task();
                THEN("the powers are returned correctly") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response = std::get<messages::GetPlatePowerResponse>(
                        tasks->get_host_comms_queue().backing_deque.front());
                    REQUIRE(response.responding_to_id == message.id);
                    REQUIRE_THAT(response.left,
                                 Catch::Matchers::WithinAbs(0.1, 0.01));
                    REQUIRE_THAT(response.center,
                                 Catch::Matchers::WithinAbs(-0.2, 0.01));
                    REQUIRE_THAT(response.right,
                                 Catch::Matchers::WithinAbs(0.3, 0.01));
                    REQUIRE_THAT(response.fans,
                                 Catch::Matchers::WithinAbs(1.0, 0.01));
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
#if defined(SYSTEM_ALLOW_ASYNC_ERRORS)
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
#endif
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());

        WHEN("sending a get-plate-temperature message") {
            auto message = messages::GetPlateTempMessage{.id = 123};
            tasks->get_thermal_plate_queue().backing_deque.push_back(
                messages::ThermalPlateMessage(message));
            tasks->run_thermal_plate_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.empty());
                AND_THEN("the task should respond with a temp of 0.0") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::GetPlateTempResponse>(
                            response));
                    auto gettemp =
                        std::get<messages::GetPlateTempResponse>(response);
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE_THAT(gettemp.current_temp,
                                 Catch::Matchers::WithinAbs(0.0, 0.1));
                    REQUIRE_THAT(gettemp.set_temp,
                                 Catch::Matchers::WithinAbs(0.0, 0.1));
                }
            }
        }
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
        WHEN("Sending a SetPlateTemperature message to enable the plate") {
            auto message = messages::SetPlateTemperatureMessage{
                .id = 123, .setpoint = 68.0F, .hold_time = 111};
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
                    AND_WHEN("sending a GetPlateTemp query") {
                        auto tempMessage =
                            messages::GetPlateTempMessage{.id = 555};
                        tasks->get_thermal_plate_queue()
                            .backing_deque.push_back(
                                messages::ThermalPlateMessage(tempMessage));
                        tasks->run_thermal_plate_task();
                        THEN("the response should have a setpoint of 0") {
                            REQUIRE(!tasks->get_host_comms_queue()
                                         .backing_deque.empty());
                            REQUIRE(std::get<messages::GetPlateTempResponse>(
                                        tasks->get_host_comms_queue()
                                            .backing_deque.front())
                                        .set_temp == 0.0F);
                        }
                    }
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
#if defined(SYSTEM_ALLOW_ASYNC_ERRORS)
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
#endif
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
    }
}
