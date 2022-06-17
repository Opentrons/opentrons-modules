#include "catch2/catch.hpp"
#include "core/pid.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/messages.hpp"
#include "test/task_builder.hpp"

constexpr double _valid_temp = 55.0;

static auto _converter =
    thermistor_conversion::Conversion<lookups::NTCG104ED104DTDSX>(
        heater_task::HeaterTask<
            TestMessageQueue>::THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
        heater_task::HeaterTask<TestMessageQueue>::ADC_BIT_DEPTH,
        heater_task::HeaterTask<
            TestMessageQueue>::HEATER_PAD_NTC_DISCONNECT_THRESHOLD_ADC);

SCENARIO("heater task message passing") {
    GIVEN("a heater task with valid temps") {
        auto tasks = TaskBuilder::build();
        auto valid_adc = _converter.backconvert(_valid_temp);
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = valid_adc, .pad_b = valid_adc, .board = valid_adc};
        tasks->get_heater_queue().backing_deque.push_back(
            messages::HeaterMessage(read_message));
        tasks->run_heater_task();
        WHEN("sending a valid set-pid-constant message") {
            auto message = messages::SetPIDConstantsMessage{
                .id = 122, .kp = 122.1, .ki = -12, .kd = 0.25};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->run_heater_task();
            THEN("the constants are updated") {
                REQUIRE_THAT(tasks->get_heater_task().get_pid().kp(),
                             Catch::Matchers::WithinAbs(122.1, 0.01));
                REQUIRE_THAT(tasks->get_heater_task().get_pid().ki(),
                             Catch::Matchers::WithinAbs(-12, 0.1));
                REQUIRE_THAT(tasks->get_heater_task().get_pid().kd(),
                             Catch::Matchers::WithinAbs(0.25, .001));
                AND_THEN("an acknowledge is sent") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto resp =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::get<messages::AcknowledgePrevious>(resp)
                                .with_error == errors::ErrorCode::NO_ERROR);
                }
            }
        }
        WHEN("sending a set-power message") {
            auto message =
                messages::SetPowerTestMessage{.id = 222, .power = 0.125};
            tasks->get_heater_queue().backing_deque.push_back(message);
            tasks->run_heater_task();
            tasks->get_heater_policy().set_power_good(true);
            tasks->get_heater_policy().set_can_reset(true);
            THEN("the task should get the message, set power, and respond") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                REQUIRE(tasks->get_heater_policy().last_power_setting() ==
                        0.125);
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .with_error == errors::ErrorCode::NO_ERROR);
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .responding_to_id == message.id);
                AND_WHEN("responding to a get-temp message") {
                    auto query_message =
                        messages::GetTemperatureMessage{.id = 14231};
                    tasks->get_heater_queue().backing_deque.push_back(
                        query_message);
                    tasks->run_heater_task();
                    THEN(
                        "the task should use the direct power set as a "
                        "setpoint response") {
                        auto query_response =
                            tasks->get_host_comms_queue().backing_deque.front();
                        tasks->get_host_comms_queue().backing_deque.pop_front();
                        REQUIRE(std::get<messages::GetTemperatureResponse>(
                                    query_response)
                                    .setpoint_temperature == 0.125);
                    }
                }
                AND_WHEN("continuing to get thermistor readings") {
                    auto conversion_message =
                        messages::TemperatureConversionComplete{
                            .pad_a = ((1U << 9) - 1),
                            .pad_b = (1U << 9),
                            .board = (1U << 11)};
                    tasks->get_heater_queue().backing_deque.push_front(
                        conversion_message);
                    tasks->run_heater_task();
                    REQUIRE(tasks->get_heater_policy().last_power_setting() ==
                            0.125);
                }
            }
        }
        WHEN("sending a set-temperature message as if from host comms") {
            auto message = messages::SetTemperatureMessage{
                .id = 1231, .target_temperature = 55};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->run_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN(
                    "the task should set red LED and respond to host comms") {
                    auto system_response =
                        tasks->get_system_queue().backing_deque.front();
                    tasks->get_system_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::SetLEDMessage>(
                        system_response));
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
                    AND_WHEN("sending a get-temperature query") {
                        auto gettemp =
                            messages::GetTemperatureMessage{.id = 1232};
                        tasks->get_heater_queue().backing_deque.push_back(
                            gettemp);
                        tasks->run_heater_task();
                        THEN("the response should have the new setpoint") {
                            REQUIRE(!tasks->get_host_comms_queue()
                                         .backing_deque.empty());
                            REQUIRE(std::get<messages::GetTemperatureResponse>(
                                        tasks->get_host_comms_queue()
                                            .backing_deque.front())
                                        .setpoint_temperature ==
                                    message.target_temperature);
                        }
                    }
                    AND_WHEN(
                        "sending a set-temperature with an out of range "
                        "value") {
                        auto message2 = messages::SetTemperatureMessage{
                            .id = 1233, .target_temperature = 105};
                        tasks->get_heater_queue().backing_deque.push_back(
                            messages::HeaterMessage(message2));
                        tasks->run_heater_task();
                        THEN(
                            "the response should indicate an out of range "
                            "error") {
                            REQUIRE(!tasks->get_host_comms_queue()
                                         .backing_deque.empty());
                            auto response2 = tasks->get_host_comms_queue()
                                                 .backing_deque.front();
                            tasks->get_host_comms_queue()
                                .backing_deque.pop_front();
                            REQUIRE(std::holds_alternative<
                                    messages::AcknowledgePrevious>(response2));
                            auto ack2 = std::get<messages::AcknowledgePrevious>(
                                response2);
                            REQUIRE(ack2.responding_to_id == message2.id);
                            REQUIRE(ack2.with_error ==
                                    errors::ErrorCode::
                                        HEATER_ILLEGAL_TARGET_TEMPERATURE);
                            AND_WHEN("sending a get-temperature query") {
                                auto gettemp =
                                    messages::GetTemperatureMessage{.id = 1234};
                                tasks->get_heater_queue()
                                    .backing_deque.push_back(gettemp);
                                tasks->run_heater_task();
                                THEN(
                                    "the response should have the old "
                                    "setpoint") {
                                    REQUIRE(!tasks->get_host_comms_queue()
                                                 .backing_deque.empty());
                                    REQUIRE(
                                        std::get<
                                            messages::GetTemperatureResponse>(
                                            tasks->get_host_comms_queue()
                                                .backing_deque.front())
                                            .setpoint_temperature ==
                                        message.target_temperature);
                                }
                            }
                        }
                    }
                    AND_WHEN("sending a deactivate-heater command") {
                        auto message2 =
                            messages::DeactivateHeaterMessage{.id = 1234};
                        tasks->get_heater_queue().backing_deque.push_back(
                            messages::HeaterMessage(message2));
                        tasks->run_heater_task();
                        THEN("the task should get the message") {
                            REQUIRE(tasks->get_heater_queue()
                                        .backing_deque.empty());
                            AND_THEN(
                                "the task should change state and respond to "
                                "host") {
                                REQUIRE(tasks->get_heater_policy()
                                            .last_enable_setting() == false);
                                REQUIRE(
                                    tasks->get_heater_task().get_setpoint() ==
                                    0.0);
                                REQUIRE(!tasks->get_host_comms_queue()
                                             .backing_deque.empty());
                                auto response = tasks->get_host_comms_queue()
                                                    .backing_deque.front();
                                tasks->get_host_comms_queue()
                                    .backing_deque.pop_front();
                                REQUIRE(std::holds_alternative<
                                        messages::AcknowledgePrevious>(
                                    response));
                                auto ack =
                                    std::get<messages::AcknowledgePrevious>(
                                        response);
                                REQUIRE(ack.responding_to_id == message2.id);
                                AND_WHEN(
                                    "sending a subsequent set-temperature "
                                    "command and valid ADC readings") {
                                    auto message3 =
                                        messages::SetTemperatureMessage{
                                            .id = 1235,
                                            .target_temperature = _valid_temp};
                                    tasks->get_heater_queue()
                                        .backing_deque.push_back(
                                            messages::HeaterMessage(message3));
                                    tasks->run_heater_task();
                                    tasks->get_heater_queue()
                                        .backing_deque.push_back(
                                            messages::HeaterMessage(
                                                read_message));
                                    tasks->run_heater_task();
                                    THEN("state should update") {
                                        REQUIRE(tasks->get_heater_policy()
                                                    .last_enable_setting() ==
                                                true);
                                        REQUIRE(tasks->get_heater_task()
                                                    .get_setpoint() ==
                                                _valid_temp);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        WHEN("sending a set-temperature message as if from system") {
            auto message = messages::SetTemperatureMessage{
                .id = 1234, .target_temperature = 55, .from_system = true};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->run_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should set red LED and respond to system") {
                    auto system_response =
                        tasks->get_system_queue().backing_deque.front();
                    tasks->get_system_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<messages::SetLEDMessage>(
                        system_response));
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
            }
        }
        WHEN("sending a get-temperature message") {
            auto message = messages::GetTemperatureMessage{.id = 999};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->run_heater_task();
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
                    REQUIRE(gettemp.setpoint_temperature == std::nullopt);
                    REQUIRE_THAT(gettemp.current_temperature,
                                 Catch::Matchers::WithinAbs(55.0, .01));
                }
            }
        }
        WHEN("sending a get-temperature-debug message") {
            auto message = messages::GetTemperatureDebugMessage{.id = 123};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->run_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the message") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::holds_alternative<
                            messages::GetTemperatureDebugResponse>(response));
                    auto gettemp =
                        std::get<messages::GetTemperatureDebugResponse>(
                            response);
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE_THAT(gettemp.pad_a_temperature,
                                 Catch::Matchers::WithinAbs(55.0, 0.1));
                    REQUIRE_THAT(gettemp.pad_b_temperature,
                                 Catch::Matchers::WithinAbs(55.0, 0.1));
                    REQUIRE_THAT(gettemp.board_temperature,
                                 Catch::Matchers::WithinAbs(55.0, 0.1));
                    REQUIRE(gettemp.pad_a_adc == valid_adc);
                    REQUIRE(gettemp.pad_b_adc == valid_adc);
                    REQUIRE(gettemp.board_adc == valid_adc);
                }
            }
        }
        WHEN(
            "setting the C offset to 6 and B offset to 1 and then re-sending "
            "the temperature readings") {
            auto offset_set_msg =
                messages::SetOffsetConstantsMessage{.id = 456,
                                                    .b_set = true,
                                                    .const_b = 1.0,
                                                    .c_set = true,
                                                    .const_c = 6.0};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(offset_set_msg));
            tasks->get_host_comms_queue().backing_deque.clear();
            tasks->run_heater_task();
            // Send temperatures to refresh calculations
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(read_message));
            tasks->run_heater_task();
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the messsage") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto ack_msg =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(ack_msg.responding_to_id == offset_set_msg.id);
                }
            }
            AND_WHEN("sending a get-temperature-debug message") {
                auto message = messages::GetTemperatureDebugMessage{.id = 123};
                tasks->get_heater_queue().backing_deque.push_back(
                    messages::HeaterMessage(message));
                tasks->get_host_comms_queue().backing_deque.clear();
                tasks->run_heater_task();
                THEN("the task should get the message") {
                    REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                    AND_THEN(
                        "the temperature should be changed by the offset") {
                        double adjusted_temp = (2.0 * _valid_temp) + 6.0F;
                        REQUIRE(!tasks->get_host_comms_queue()
                                     .backing_deque.empty());
                        auto response =
                            tasks->get_host_comms_queue().backing_deque.front();
                        tasks->get_host_comms_queue().backing_deque.pop_front();
                        REQUIRE(std::holds_alternative<
                                messages::GetTemperatureDebugResponse>(
                            response));
                        auto gettemp =
                            std::get<messages::GetTemperatureDebugResponse>(
                                response);

                        REQUIRE(gettemp.responding_to_id == message.id);

                        REQUIRE_THAT(
                            gettemp.board_temperature,
                            Catch::Matchers::WithinAbs(_valid_temp, 0.1));
                        REQUIRE(gettemp.board_adc == valid_adc);

                        REQUIRE_THAT(
                            gettemp.pad_a_temperature,
                            Catch::Matchers::WithinAbs(adjusted_temp, 0.01));
                        REQUIRE(gettemp.pad_a_adc == valid_adc);

                        REQUIRE_THAT(
                            gettemp.pad_b_temperature,
                            Catch::Matchers::WithinAbs(adjusted_temp, 0.01));
                        REQUIRE(gettemp.pad_b_adc == valid_adc);
                    }
                }
            }
            AND_WHEN("sending a get-offset-constants message") {
                auto get_offsets =
                    messages::GetOffsetConstantsMessage{.id = 654};
                tasks->get_heater_queue().backing_deque.push_back(
                    messages::HeaterMessage(get_offsets));
                tasks->get_host_comms_queue().backing_deque.clear();
                tasks->run_heater_task();
                THEN("the task should get the message") {
                    REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                    AND_THEN("the response should have B=1 and C=6") {
                        REQUIRE(!tasks->get_host_comms_queue()
                                     .backing_deque.empty());
                        auto response =
                            tasks->get_host_comms_queue().backing_deque.front();
                        tasks->get_host_comms_queue().backing_deque.pop_front();
                        REQUIRE(std::holds_alternative<
                                messages::GetOffsetConstantsResponse>(
                            response));
                        auto constants =
                            std::get<messages::GetOffsetConstantsResponse>(
                                response);
                        REQUIRE(constants.responding_to_id == get_offsets.id);
                        REQUIRE_THAT(constants.const_b,
                                     Catch::Matchers::WithinAbs(1.0F, 0.01F));
                        REQUIRE_THAT(constants.const_c,
                                     Catch::Matchers::WithinAbs(6.0F, 0.01F));
                    }
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
        tasks->run_heater_task();
        CHECK(std::holds_alternative<messages::ErrorMessage>(
            tasks->get_host_comms_queue().backing_deque.front()));
        tasks->get_host_comms_queue().backing_deque.pop_front();
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
        WHEN("sending a set-temperature message") {
            auto message = messages::SetTemperatureMessage{
                .id = 1231, .target_temperature = 60};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->run_heater_task();
            THEN("the task should respond with an error") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto ack = std::get<messages::AcknowledgePrevious>(response);
                REQUIRE(ack.responding_to_id == message.id);
                REQUIRE(ack.with_error ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_SHORT);
            }
        }
        WHEN("sending a get-temperature message") {
            auto message = messages::GetTemperatureMessage{.id = 2222};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(message));
            tasks->run_heater_task();
            THEN("the task should respond with an error") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto ack = std::get<messages::GetTemperatureResponse>(response);
                REQUIRE(ack.responding_to_id == message.id);
                REQUIRE(ack.with_error ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_SHORT);
            }
        }
    }
}

SCENARIO("heater task error handling") {
    GIVEN("a heater task with no errors") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = ((1U << 9) - 1), .pad_b = (1U << 9), .board = (1U << 11)};
        tasks->get_heater_queue().backing_deque.push_back(
            messages::HeaterMessage(read_message));
        tasks->run_heater_task();
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
        WHEN("setting thermistor a to an error state and setting the latch") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = 0, .pad_b = (1U << 9), .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(one_error_message));
            tasks->run_heater_task();
            THEN(
                "one error message should be sent indicating the pad sense "
                "error") {
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                auto error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_A_SHORT);
            }
        }
        WHEN("setting thermistor b to an error state and setting the latch") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = ((1U << 9) - 1), .pad_b = 0, .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(one_error_message));
            tasks->run_heater_task();
            THEN(
                "one error message should be sent indicating the pad sense "
                "error") {
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                auto error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_SHORT);
            }
        }
        WHEN(
            "setting both thermistors to an error state and setting the "
            "latch") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = 0, .pad_b = ((1U << 12) - 1), .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(one_error_message));
            tasks->run_heater_task();
            THEN("one error message should be sent for each pad sense error") {
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_A_SHORT);
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED);
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
            }
        }
        WHEN(
            "simulating an NTC disconnect by setting both thermistors to an "
            "error state and setting the "
            "latch") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = ((1U << 12) - 1),
                .pad_b = ((1U << 12) - 1),
                .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(one_error_message));
            tasks->run_heater_task();
            THEN("one error message should be sent for each pad sense error") {
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_A_DISCONNECTED);
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED);
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN(
                    "simulating an NTC reconnect by setting both thermistors "
                    "back in range") {
                    tasks->get_heater_policy().set_can_reset(true);
                    tasks->get_heater_policy().reset_try_reset_call_count();
                    auto one_error_message =
                        messages::TemperatureConversionComplete{
                            .pad_a = (1U << 11),
                            .pad_b = (1U << 11),
                            .board = (1U << 11)};
                    tasks->get_heater_queue().backing_deque.push_back(
                        messages::HeaterMessage(one_error_message));
                    tasks->run_heater_task();
                    THEN("latch should reset") {
                        CHECK(tasks->get_host_comms_queue()
                                  .backing_deque.empty());
                        REQUIRE(
                            tasks->get_heater_policy().try_reset_call_count() ==
                            1);
                        REQUIRE(tasks->get_heater_policy().power_good());
                    }
                }
            }
        }
        WHEN(
            "simulating an overtemp error in hardware (by setting "
            "HEAT_POWER_GOOD low) and software (by setting adc values to ~102 "
            "degrees C)") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = 422, .pad_b = 422, .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(one_error_message));
            tasks->run_heater_task();
            THEN("one error message should be sent for each pad sense error") {
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                auto error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_A_OVERTEMP);
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_THERMISTOR_B_OVERTEMP);
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN(
                    "simulating that the temp has dropped below the latch "
                    "reset temp") {
                    tasks->get_heater_policy().set_can_reset(true);
                    tasks->get_heater_policy().reset_try_reset_call_count();
                    auto one_error_message =
                        messages::TemperatureConversionComplete{
                            .pad_a = (1U << 11),
                            .pad_b = (1U << 11),
                            .board = (1U << 11)};
                    tasks->get_heater_queue().backing_deque.push_back(
                        messages::HeaterMessage(one_error_message));
                    tasks->run_heater_task();
                    THEN("latch should reset") {
                        CHECK(tasks->get_host_comms_queue()
                                  .backing_deque.empty());
                        REQUIRE(
                            tasks->get_heater_policy().try_reset_call_count() ==
                            1);
                        REQUIRE(tasks->get_heater_policy().power_good());
                    }
                }
            }
        }
        WHEN(
            "setting both thermistors to ok values but indicating a latched "
            "error") {
            tasks->get_heater_policy().set_power_good(false);
            tasks->get_heater_policy().set_can_reset(false);
            tasks->get_heater_policy().reset_try_reset_call_count();
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(read_message));
            tasks->run_heater_task();
            THEN(
                "an error message should be sent and we should be in error "
                "state") {
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto error_update =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                CHECK(tasks->get_host_comms_queue().backing_deque.empty());
                auto error = std::get<messages::ErrorMessage>(error_update);
                REQUIRE(error.code ==
                        errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
                auto set_temp_message = messages::SetTemperatureMessage{
                    .id = 24, .target_temperature = 29.2};
                tasks->get_heater_queue().backing_deque.push_back(
                    messages::HeaterMessage(set_temp_message));
                tasks->run_heater_task();
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .with_error ==
                        errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
                CHECK(tasks->get_host_comms_queue().backing_deque.empty());
            }
        }
    }
    GIVEN("a heater task with a thermistor reading something bad") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = 0, .pad_b = (1U << 15), .board = (1U << 11)};
        tasks->get_heater_queue().backing_deque.push_back(
            messages::HeaterMessage(read_message));
        tasks->get_heater_policy().set_power_good(false);
        tasks->run_heater_task();
        CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
        tasks->get_host_comms_queue().backing_deque.clear();
        WHEN("the error goes away and the latch is allowed to be reset") {
            read_message = messages::TemperatureConversionComplete{
                .pad_a = ((1U << 9) - 1),
                .pad_b = (1U << 9),
                .board = (1U << 11)};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(read_message));
            tasks->run_heater_task();
            THEN("there is no error and the heater task works normally") {
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                auto settemp = messages::SetTemperatureMessage{
                    .id = 54, .target_temperature = 43};
                tasks->get_heater_queue().backing_deque.push_back(settemp);
                tasks->run_heater_task();
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .with_error == errors::ErrorCode::NO_ERROR);
            }
        }
        WHEN("the pad error goes away but the latch cannot reset") {
            auto read_message = messages::TemperatureConversionComplete{
                .pad_a = ((1U << 9) - 1),
                .pad_b = (1U << 9),
                .board = (1U << 11)};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(read_message));
            tasks->get_heater_policy().set_can_reset(false);
            tasks->run_heater_task();
            THEN("there is still an error and it is sent appropriately") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto pgood_error =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                REQUIRE(std::get<messages::ErrorMessage>(pgood_error).code ==
                        errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
                AND_WHEN("sending a set temp") {
                    auto settemp = messages::SetTemperatureMessage{
                        .id = 54, .target_temperature = 43};
                    tasks->get_heater_queue().backing_deque.push_back(settemp);
                    tasks->run_heater_task();
                    THEN("there is an error response") {
                        CHECK(!tasks->get_host_comms_queue()
                                   .backing_deque.empty());
                        auto response =
                            tasks->get_host_comms_queue().backing_deque.front();
                        tasks->get_host_comms_queue().backing_deque.pop_front();
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(
                            std::get<messages::AcknowledgePrevious>(response)
                                .with_error ==
                            errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
                    }
                }
            }
        }
    }

    GIVEN("a heater task with thermistors reading ok but error latch set") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = ((1U << 9) - 1), .pad_b = (1U << 9), .board = (1U << 11)};
        tasks->get_heater_queue().backing_deque.push_back(
            messages::HeaterMessage(read_message));
        tasks->get_heater_policy().set_power_good(false);
        tasks->get_heater_policy().set_can_reset(false);
        tasks->run_heater_task();
        tasks->get_host_comms_queue().backing_deque.clear();
        WHEN("sending a set-temp with the latch allowed to reset") {
            tasks->get_heater_policy().set_can_reset(true);
            tasks->get_heater_policy().reset_try_reset_call_count();
            auto settemp = messages::SetTemperatureMessage{
                .id = 254, .target_temperature = 54};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(settemp));
            tasks->run_heater_task();
            THEN("the set temp should reset the latch and succeed") {
                REQUIRE(tasks->get_heater_policy().try_reset_call_count() == 1);
                REQUIRE(tasks->get_heater_policy().power_good());
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .with_error == errors::ErrorCode::NO_ERROR);
            }
        }
        WHEN("sending a set-temp with the latch not allowed to reset") {
            tasks->get_heater_policy().set_can_reset(false);
            tasks->get_heater_policy().reset_try_reset_call_count();
            auto settemp = messages::SetTemperatureMessage{
                .id = 254, .target_temperature = 54};
            tasks->get_heater_queue().backing_deque.push_back(
                messages::HeaterMessage(settemp));
            tasks->run_heater_task();
            THEN(
                "the set temp should try and fail to reset the latch and send "
                "an error") {
                REQUIRE(tasks->get_heater_policy().try_reset_call_count() == 1);
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                REQUIRE(std::get<messages::AcknowledgePrevious>(response)
                            .with_error ==
                        errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
            }
        }
    }
}
