#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-gen2/errors.hpp"
#include "thermocycler-gen2/lid_heater_task.hpp"
#include "thermocycler-gen2/messages.hpp"

constexpr int _valid_adc = 6360;  // Gives 50C
constexpr double _valid_temp = 50.0;
constexpr int _shorted_adc = 0;
constexpr int _disconnected_adc = 0x5DC0;
constexpr uint32_t TIME_DELTA =
    lid_heater_task::LidHeaterTask<TestMessageQueue>::CONTROL_PERIOD_TICKS;

SCENARIO("lid heater task message passing") {
    uint32_t timestamp = TIME_DELTA;
    GIVEN("a lid heater task with valid temps") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::LidTempReadComplete{
            .lid_temp = _valid_adc, .timestamp_ms = timestamp};
        timestamp += TIME_DELTA;
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
                    auto gettemp = tasks->get_latest_host_comms_message<
                        messages::GetLidTemperatureDebugResponse>();
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
                    auto gettemp = tasks->get_latest_host_comms_message<
                        messages::GetLidTempResponse>();
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
                    tasks->require_has_ack_for(message);
                    REQUIRE(tasks->get_lid_heater_policy().get_heater_power() ==
                            0.65);
                }
            }
        }
        WHEN("Sending a SetPIDConstants to configure the lid constants") {
            auto message = messages::SetPIDConstantsMessage{
                .id = 123,
                .selection = PidSelection::HEATER,
                .p = 1,
                .i = 1,
                .d = 1};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should act on the message") {
                    REQUIRE(
                        tasks->get_thermal_plate_queue().backing_deque.empty());
                    tasks->require_has_ack_for(message);
                }
            }
        }
        WHEN("Sending a SetPIDConstants with invalid constants") {
            auto message = messages::SetPIDConstantsMessage{
                .id = 555,
                .selection = PidSelection::HEATER,
                .p = 1000,
                .i = 1,
                .d = 1};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should act on the message") {
                    REQUIRE(
                        tasks->get_thermal_plate_queue().backing_deque.empty());
                    tasks->require_has_ack_for(
                        message,
                        errors::ErrorCode::THERMAL_CONSTANT_OUT_OF_RANGE);
                }
            }
        }
        WHEN("Sending a SetLidTemperature message to enable the lid") {
            auto message = messages::SetLidTemperatureMessage{
                .id = 123, .setpoint = 100.0F};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the message") {
                    tasks->require_has_ack_for(message);
                    AND_WHEN("sending a GetLidTemp query") {
                        auto tempMessage =
                            messages::GetLidTempMessage{.id = 555};
                        tasks->get_lid_heater_queue().backing_deque.push_back(
                            messages::LidHeaterMessage(tempMessage));
                        tasks->run_lid_heater_task();
                        THEN("the response should have the new setpoint") {
                            auto response =
                                tasks->get_latest_host_comms_message<
                                    messages::GetLidTempResponse>();
                            REQUIRE(response.set_temp == message.setpoint);
                        }
                    }
                }
                AND_WHEN("sending updated temperatures below target") {
                    read_message.timestamp_ms = timestamp;
                    timestamp += TIME_DELTA;
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        messages::LidHeaterMessage(read_message));
                    tasks->run_lid_heater_task();
                    THEN("the peltiers should be enabled") {
                        auto power =
                            tasks->get_lid_heater_policy().get_heater_power();
                        REQUIRE(power > 0.0F);
                    }
                }
            }
            AND_WHEN("sending a DeactivateLidHeating command") {
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto tempMessage =
                    messages::DeactivateLidHeatingMessage{.id = 321};
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    messages::LidHeaterMessage(tempMessage));
                tasks->run_lid_heater_task();
                THEN("the task should respond to the message") {
                    tasks->require_has_ack_for(tempMessage);
                    AND_WHEN("sending a GetLidTemp query") {
                        auto tempMessage =
                            messages::GetLidTempMessage{.id = 555};
                        tasks->get_lid_heater_queue().backing_deque.push_back(
                            messages::LidHeaterMessage(tempMessage));
                        tasks->run_lid_heater_task();
                        THEN("the response should have no setpoint") {
                            auto response =
                                tasks->get_latest_host_comms_message<
                                    messages::GetLidTempResponse>();
                            REQUIRE(response.set_temp == 0.0F);
                        }
                    }
                }
            }
            AND_WHEN(
                "sending a DeactivateLidHeating command from system task") {
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto tempMessage = messages::DeactivateLidHeatingMessage{
                    .id = 321, .from_system = true};
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    tempMessage);
                tasks->run_lid_heater_task();
                THEN("the task should respond to the message") {
                    REQUIRE_FALSE(
                        tasks->get_system_queue().backing_deque.empty());
                    auto response =
                        tasks->get_system_queue().backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto ack =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(ack.responding_to_id == tempMessage.id);
                    REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
                }
            }
            AND_WHEN("sending a DeactivateAll command") {
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto tempMessage = messages::DeactivateAllMessage{.id = 321};
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    messages::LidHeaterMessage(tempMessage));
                tasks->run_lid_heater_task();
                THEN("the task should respond to the message") {
                    auto response = tasks->get_latest_host_comms_message<
                        messages::DeactivateAllResponse>();
                    REQUIRE(response.responding_to_id == 321);
                    AND_WHEN("sending a GetLidTemp query") {
                        auto tempMessage =
                            messages::GetLidTempMessage{.id = 555};
                        tasks->get_lid_heater_queue().backing_deque.push_back(
                            messages::LidHeaterMessage(tempMessage));
                        tasks->run_lid_heater_task();
                        THEN("the response should have no setpoint") {
                            auto gettemp = tasks->get_latest_host_comms_message<
                                messages::GetLidTempResponse>();
                            REQUIRE(gettemp.set_temp == 0.0F);
                        }
                    }
                }
            }
            AND_WHEN(
                "Sending a SetPIDConstants to configure the lid constants") {
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto message = messages::SetPIDConstantsMessage{
                    .id = 808,
                    .selection = PidSelection::HEATER,
                    .p = 1,
                    .i = 1,
                    .d = 1};
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    messages::LidHeaterMessage(message));
                tasks->run_lid_heater_task();
                THEN("the task should get the message") {
                    REQUIRE(
                        tasks->get_lid_heater_queue().backing_deque.empty());
                    AND_THEN("the task should respond with a busy error") {
                        REQUIRE(tasks->get_thermal_plate_queue()
                                    .backing_deque.empty());
                        tasks->require_has_ack_for(
                            message, errors::ErrorCode::THERMAL_LID_BUSY);
                    }
                }
            }
        }
        GIVEN("some power on the heater") {
            tasks->get_lid_heater_policy().set_heater_power(0.5);
            WHEN("sending GetThermalPowerMessage") {
                auto message = messages::GetThermalPowerMessage{.id = 123};
                tasks->get_lid_heater_queue().backing_deque.push_back(message);
                tasks->run_lid_heater_task();
                THEN("the power is returned correctly") {
                    auto response = tasks->get_latest_host_comms_message<
                        messages::GetLidPowerResponse>();
                    REQUIRE(response.responding_to_id == message.id);
                    REQUIRE_THAT(response.heater,
                                 Catch::Matchers::WithinAbs(0.5, 0.01));
                }
            }
        }
    }
    GIVEN("a heater task with a shorted temp") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::LidTempReadComplete{
            .lid_temp = _shorted_adc, .timestamp_ms = timestamp};
        timestamp += TIME_DELTA;
        tasks->get_lid_heater_queue().backing_deque.push_back(
            messages::LidHeaterMessage(read_message));
        tasks->run_lid_heater_task();
#if defined(SYSTEM_ALLOW_ASYNC_ERRORS)
        auto error_msg =
            tasks->get_latest_host_comms_message<messages::ErrorMessage>();
        CHECK(error_msg.code == errors::ErrorCode::THERMISTOR_LID_SHORT);
#endif
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());

        WHEN("sending a get error status message") {
            auto message = messages::GetErrorStateMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(message);
            tasks->run_lid_heater_task();
            THEN("the task should respond with an error") {
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::THERMISTOR_LID_SHORT);
            }
        }

        WHEN("Sending a SetHeaterDebug message to enable the heater") {
            auto message =
                messages::SetHeaterDebugMessage{.id = 124, .power = 0.65};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should respond with an error") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                REQUIRE(tasks->get_lid_heater_policy().get_heater_power() ==
                        0.0F);
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::THERMISTOR_LID_SHORT);
            }
        }
        WHEN("Sending a SetLidTemperature message to enable the lid") {
            auto message = messages::SetLidTemperatureMessage{
                .id = 123, .setpoint = 100.0F};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should respond with an error") {
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::THERMISTOR_LID_SHORT);
                AND_WHEN("sending a GetLidTemp query") {
                    auto tempMessage = messages::GetLidTempMessage{.id = 555};
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        messages::LidHeaterMessage(tempMessage));
                    tasks->run_lid_heater_task();
                    THEN("the response should have a setpoint of 0") {
                        auto gettemp = tasks->get_latest_host_comms_message<
                            messages::GetLidTempResponse>();
                        REQUIRE(gettemp.set_temp == 0.0F);
                    }
                }
            }
        }
        WHEN("sending a get-lid-temperature message") {
            auto message = messages::GetLidTempMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should respond with a temp of 0") {
                auto gettemp = tasks->get_latest_host_comms_message<
                    messages::GetLidTempResponse>();
                REQUIRE(gettemp.responding_to_id == message.id);
                REQUIRE_THAT(gettemp.current_temp,
                             Catch::Matchers::WithinAbs(0.0, 0.1));
                REQUIRE_THAT(gettemp.set_temp,
                             Catch::Matchers::WithinAbs(0.0, 0.1));
            }
        }
    }
    GIVEN("a heater task with a disconnected thermistor") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::LidTempReadComplete{
            .lid_temp = _disconnected_adc, .timestamp_ms = timestamp};
        timestamp += TIME_DELTA;
        tasks->get_lid_heater_queue().backing_deque.push_back(
            messages::LidHeaterMessage(read_message));
        tasks->run_lid_heater_task();
#if defined(SYSTEM_ALLOW_ASYNC_ERRORS)
        auto error_msg =
            tasks->get_latest_host_comms_message<messages::ErrorMessage>();
        CHECK(error_msg.code == errors::ErrorCode::THERMISTOR_LID_DISCONNECTED);
#endif
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
        CHECK(tasks->get_lid_heater_task().most_relevant_error() ==
              errors::ErrorCode::THERMISTOR_LID_DISCONNECTED);

        WHEN("Sending a SetHeaterDebug message to enable the heater") {
            auto message =
                messages::SetHeaterDebugMessage{.id = 124, .power = 0.65};
            tasks->get_lid_heater_queue().backing_deque.push_back(
                messages::LidHeaterMessage(message));
            tasks->run_lid_heater_task();
            THEN("the task should respond with an error") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.empty());
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::THERMISTOR_LID_DISCONNECTED);
                REQUIRE(tasks->get_lid_heater_policy().get_heater_power() ==
                        0.0F);
            }
        }
        WHEN("it naturally clears") {
            read_message.lid_temp = _valid_adc;
            timestamp += TIME_DELTA;
            tasks->get_lid_heater_queue().backing_deque.push_back(read_message);
            tasks->run_lid_heater_task();
            THEN("the error is cleared") {
                REQUIRE(tasks->get_lid_heater_task().most_relevant_error() ==
                        errors::ErrorCode::NO_ERROR);
            }
            THEN("a get error still gets it") {
                tasks->get_host_comms_queue().backing_deque.clear();
                auto get_message = messages::GetErrorStateMessage{.id = 222};
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    get_message);
                tasks->run_lid_heater_task();
                tasks->require_has_ack_for(
                    get_message,
                    errors::ErrorCode::THERMISTOR_LID_DISCONNECTED);
                AND_THEN("a subsequent get error doesn't") {
                    get_message.id = 223;
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        get_message);
                    tasks->run_lid_heater_task();
                    tasks->require_has_ack_for(get_message);
                }
            }
        }
        WHEN("Sending a get error state message") {
            auto get_message = messages::GetErrorStateMessage{.id = 333};
            tasks->get_lid_heater_queue().backing_deque.push_back(get_message);
            tasks->run_lid_heater_task();
            THEN("the acknowledgement has the error") {
                tasks->require_has_ack_for(
                    get_message,
                    errors::ErrorCode::THERMISTOR_LID_DISCONNECTED);
            }
        }
        WHEN("sending a clear error state message") {
            auto message = messages::ClearErrorStateMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(message);
            tasks->run_lid_heater_task();
            AND_WHEN("the error is not physically cleared") {
                read_message.timestamp_ms += TIME_DELTA;
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    read_message);
                tasks->run_lid_heater_task();
                tasks->get_host_comms_queue().backing_deque.clear();
                THEN("the error reoccurs") {
                    auto message = messages::GetErrorStateMessage{.id = 333};
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        message);
                    tasks->run_lid_heater_task();
                    tasks->require_has_ack_for(
                        message,
                        errors::ErrorCode::THERMISTOR_LID_DISCONNECTED);
                }
            }
            AND_WHEN("the error is physically cleared") {
                read_message.timestamp_ms += TIME_DELTA;
                read_message.lid_temp = _valid_adc;
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    read_message);
                tasks->run_lid_heater_task();
                tasks->get_host_comms_queue().backing_deque.clear();
                THEN("the error is cleared") {
                    auto message = messages::GetErrorStateMessage{.id = 333};
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        message);
                    tasks->run_lid_heater_task();
                    tasks->require_has_ack_for(message);
                }
            }
        }
    }
    WHEN("sending SetLidFans message") {
        auto tasks = TaskBuilder::build();
        REQUIRE(!tasks->get_lid_heater_policy().lid_fans_enabled());
        auto fan_msg = messages::SetLidFansMessage{.id = 123, .enable = true};
        tasks->get_lid_heater_queue().backing_deque.push_back(fan_msg);
        tasks->run_lid_heater_task();
        THEN("the lid fan is enabled") {
            REQUIRE(tasks->get_lid_heater_policy().lid_fans_enabled());
        }
        THEN("the message is acked") { tasks->require_has_ack_for(fan_msg); }
        AND_WHEN("sending another message to disable the fans") {
            fan_msg.id = 456;
            fan_msg.enable = false;
            tasks->get_host_comms_queue().backing_deque.clear();

            tasks->get_lid_heater_queue().backing_deque.push_back(fan_msg);
            tasks->run_lid_heater_task();
            THEN("the lid fan is disabled") {
                REQUIRE(!tasks->get_lid_heater_policy().lid_fans_enabled());
            }
            THEN("the message is acked") {
                tasks->require_has_ack_for(fan_msg);
            }
        }
    }
}

SCENARIO("lid heater error flag handling") {
    uint32_t timestamp = TIME_DELTA;
    GIVEN("a lid heater task with invalid temperatures") {
        auto tasks = TaskBuilder::build();
        auto &lid_queue = tasks->get_lid_heater_queue();
        auto &host_queue = tasks->get_host_comms_queue();
        auto read_message = messages::LidTempReadComplete{
            .lid_temp = _shorted_adc, .timestamp_ms = timestamp};
        timestamp += TIME_DELTA;
        lid_queue.backing_deque.push_back(read_message);
        tasks->run_lid_heater_task();
        CHECK(tasks->get_lid_heater_task().most_relevant_error() ==
              errors::ErrorCode::THERMISTOR_LID_SHORT);
        WHEN("it naturally clears") {
            read_message.lid_temp = _valid_adc;
            timestamp += TIME_DELTA;
            tasks->get_lid_heater_queue().backing_deque.push_back(read_message);
            tasks->run_lid_heater_task();
            THEN("the error is cleared") {
                REQUIRE(tasks->get_lid_heater_task().most_relevant_error() ==
                        errors::ErrorCode::NO_ERROR);
            }
            THEN("a get error still gets it") {
                tasks->get_host_comms_queue().backing_deque.clear();
                auto get_message = messages::GetErrorStateMessage{.id = 222};
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    get_message);
                tasks->run_lid_heater_task();
                tasks->require_has_ack_for(
                    get_message, errors::ErrorCode::THERMISTOR_LID_SHORT);
                AND_THEN("a subsequent get error doesn't") {
                    get_message.id = 223;
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        get_message);
                    tasks->run_lid_heater_task();
                    tasks->require_has_ack_for(get_message);
                }
            }
        }

        WHEN("sending a get error status message") {
            auto message = messages::GetErrorStateMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(message);
            tasks->run_lid_heater_task();
            THEN("the task should respond with an error") {
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::THERMISTOR_LID_SHORT);
            }
        }
        WHEN("sending a clear error state message") {
            auto message = messages::ClearErrorStateMessage{.id = 123};
            tasks->get_lid_heater_queue().backing_deque.push_back(message);
            tasks->run_lid_heater_task();
            AND_WHEN("the error is not physically cleared") {
                read_message.timestamp_ms += TIME_DELTA;
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    read_message);
                tasks->run_lid_heater_task();
                tasks->get_host_comms_queue().backing_deque.clear();
                THEN("the error reoccurs") {
                    auto message = messages::GetErrorStateMessage{.id = 333};
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        message);
                    tasks->run_lid_heater_task();
                    tasks->require_has_ack_for(
                        message, errors::ErrorCode::THERMISTOR_LID_SHORT);
                }
            }
            AND_WHEN("the error is physically cleared") {
                read_message.timestamp_ms += TIME_DELTA;
                read_message.lid_temp = _valid_adc;
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    read_message);
                tasks->run_lid_heater_task();
                tasks->get_host_comms_queue().backing_deque.clear();
                THEN("the error is cleared") {
                    auto message = messages::GetErrorStateMessage{.id = 333};
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        message);
                    tasks->run_lid_heater_task();
                    tasks->require_has_ack_for(message);
                }
            }
        }
        WHEN("sending a SetPlateTemperature message") {
            auto set_msg =
                messages::SetLidTemperatureMessage{.id = 123, .setpoint = 50};
            lid_queue.backing_deque.push_back(set_msg);
            tasks->run_lid_heater_task();
            THEN("the response shows an error") {
                tasks->require_has_ack_for(
                    set_msg, errors::ErrorCode::THERMISTOR_LID_SHORT);
            }
        }
        WHEN("sending a DeactivateAll message") {
            auto deactivate = messages::DeactivateAllMessage{.id = 444};
            lid_queue.backing_deque.push_back(deactivate);
            tasks->run_lid_heater_task();
            host_queue.backing_deque.clear();
            AND_THEN("sending a SetPlateTemperature message") {
                auto set_msg = messages::SetLidTemperatureMessage{
                    .id = 123, .setpoint = 50};
                lid_queue.backing_deque.push_back(set_msg);
                tasks->run_lid_heater_task();
                THEN("the response shows an error") {
                    tasks->require_has_ack_for(
                        set_msg, errors::ErrorCode::THERMISTOR_LID_SHORT);
                }
            }
        }
    }
}

SCENARIO("lid set-error") {
    uint32_t timestamp = 1000;
    GIVEN("a good lid heater task") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::LidTempReadComplete{
            .lid_temp = _valid_adc, .timestamp_ms = timestamp};
        tasks->get_lid_heater_queue().backing_deque.push_back(
            messages::LidHeaterMessage(read_message));
        tasks->run_lid_heater_task();
        tasks->get_host_comms_queue().backing_deque.clear();
        auto error_message = messages::SetErrorStateMessage{
            .id = 333,
            .error_to_set = errors::ErrorCode::THERMAL_HEATER_ERROR,
            .delay_s = 5};
        tasks->get_lid_heater_queue().backing_deque.push_back(error_message);
        tasks->run_lid_heater_task();
        tasks->require_has_ack_for(error_message);

        THEN("after just less than the time the error is not set") {
            read_message.timestamp_ms = 5999;
            tasks->get_lid_heater_queue().backing_deque.push_back(read_message);
            tasks->run_lid_heater_task();
            REQUIRE(tasks->get_lid_heater_task().most_relevant_error() ==
                    errors::ErrorCode::NO_ERROR);
            auto get_message = messages::GetErrorStateMessage{.id = 100};
            tasks->get_lid_heater_queue().backing_deque.push_back(get_message);
            tasks->run_lid_heater_task();
            tasks->require_has_ack_for(get_message);
        }
        THEN("after exactly the time the error is set") {
            read_message.timestamp_ms = 6000;
            tasks->get_lid_heater_queue().backing_deque.push_back(read_message);
            tasks->run_lid_heater_task();
#if defined(SYSTEM_ALLOW_ASYNC_ERRORS)
            REQUIRE(
                tasks->get_latest_host_comms_message<messages::ErrorMessage>()
                    .error == errors::ErrorCode::THERMAL_HEATER_ERROR);

#endif
            REQUIRE(tasks->get_lid_heater_task().most_relevant_error() ==
                    errors::ErrorCode::THERMAL_HEATER_ERROR);
            auto get_message = messages::GetErrorStateMessage{.id = 100};
            tasks->get_lid_heater_queue().backing_deque.push_back(get_message);
            tasks->run_lid_heater_task();
            tasks->require_has_ack_for(get_message,
                                       errors::ErrorCode::THERMAL_HEATER_ERROR);
        }
        THEN("after more than the time the error is set") {
            read_message.timestamp_ms = 10000;
            tasks->get_lid_heater_queue().backing_deque.push_back(read_message);
            tasks->run_lid_heater_task();
#if defined(SYSTEM_ALLOW_ASYNC_ERRORS)
            REQUIRE(
                tasks->get_latest_host_comms_message<messages::ErrorMessage>()
                    .error == errors::ErrorCode::THERMAL_HEATER_ERROR);

#endif
            REQUIRE(tasks->get_lid_heater_task().most_relevant_error() ==
                    errors::ErrorCode::THERMAL_HEATER_ERROR);
            auto get_message = messages::GetErrorStateMessage{.id = 100};
            tasks->get_lid_heater_queue().backing_deque.push_back(get_message);
            tasks->run_lid_heater_task();
            tasks->require_has_ack_for(get_message,
                                       errors::ErrorCode::THERMAL_HEATER_ERROR);
        }
        auto error = GENERATE(errors::ErrorCode::THERMISTOR_LID_SHORT,
                              errors::ErrorCode::THERMISTOR_LID_DISCONNECTED,
                              errors::ErrorCode::THERMISTOR_LID_OVERTEMP,
                              errors::ErrorCode::THERMAL_HEATER_ERROR);
        WHEN(std::string("Setting error ") +
             std::to_string(static_cast<uint32_t>(error))) {
            auto set_message = messages::SetErrorStateMessage{
                .id = 1231, .error_to_set = error, .delay_s = 0};
            tasks->get_lid_heater_queue().backing_deque.push_back(set_message);
            tasks->run_lid_heater_task();
            tasks->require_has_ack_for(set_message);
            read_message.timestamp_ms = 2000;
            tasks->get_lid_heater_queue().backing_deque.push_back(read_message);
            tasks->run_lid_heater_task();
#if defined(SYSTEM_ALLOW_ASYNC_ERRORS)
            THEN("the error is reported asynchronously") {
                auto error_msg = tasks->get_latest_host_comms_message<
                    messages::ErrorMessage>();
                REQUIRE(error_msg.error == error);
                plate_queue.backing_deque.pop_front();
            }
#endif
            THEN("the error is reported by a get-error-state") {
                auto get_state = messages::GetErrorStateMessage{.id = 2222};
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    get_state);
                tasks->run_lid_heater_task();
                tasks->require_has_ack_for(get_state, error);
            }
            AND_WHEN("clearing the error and updating the state") {
                auto clear = messages::ClearErrorStateMessage{.id = 99};
                tasks->get_lid_heater_queue().backing_deque.push_back(clear);
                tasks->run_lid_heater_task();
                tasks->require_has_ack_for(clear);
                read_message.timestamp_ms = 3000;
                tasks->get_lid_heater_queue().backing_deque.push_back(
                    read_message);
                tasks->run_lid_heater_task();
                THEN("the error is gone") {
                    auto get_state = messages::GetErrorStateMessage{.id = 3333};
                    tasks->get_lid_heater_queue().backing_deque.push_back(
                        get_state);
                    tasks->run_lid_heater_task();
                    tasks->require_has_ack_for(get_state);
                }
            }
        }
    }
}
