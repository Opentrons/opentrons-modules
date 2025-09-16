#include "catch2/catch.hpp"
#include "core/pid.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/messages.hpp"
#include "test/task_builder.hpp"

constexpr double _valid_temp = 55.0;
constexpr double _thermal_offset_B = -0.0259;
constexpr double _thermal_offset_C = 0.6755;
constexpr double _valid_temp_offset =
    (1 + _thermal_offset_B) * _valid_temp + _thermal_offset_C;

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
        tasks->consume_heater_message(read_message);
        WHEN("sending a valid set-pid-constant message") {
            auto message = messages::SetPIDConstantsMessage{
                .id = 122, .kp = 122.1, .ki = -12, .kd = 0.25};
            tasks->consume_heater_message(message);
            THEN("the constants are updated") {
                REQUIRE_THAT(tasks->get_heater_task().get_pid().kp(),
                             Catch::Matchers::WithinAbs(122.1, 0.01));
                REQUIRE_THAT(tasks->get_heater_task().get_pid().ki(),
                             Catch::Matchers::WithinAbs(-12, 0.1));
                REQUIRE_THAT(tasks->get_heater_task().get_pid().kd(),
                             Catch::Matchers::WithinAbs(0.25, .001));
                AND_THEN("an acknowledge is sent") {
                    tasks->require_has_ack_for(message);
                }
            }
        }
        WHEN("sending a set-power message") {
            auto message =
                messages::SetPowerTestMessage{.id = 222, .power = 0.125};
            tasks->consume_heater_message(message);
            tasks->get_heater_policy().set_power_good(true);
            tasks->get_heater_policy().set_can_reset(true);
            THEN("the task should get the message, set power, and respond") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                REQUIRE(tasks->get_heater_policy().last_power_setting() ==
                        0.125);
                tasks->require_has_ack_for(message);
                AND_WHEN("responding to a get-temp message") {
                    auto query_message =
                        messages::GetTemperatureMessage{.id = 14231};
                    tasks->consume_heater_message(query_message);
                    THEN(
                        "the task should use the direct power set as a "
                        "setpoint response") {
                        auto query_response =
                            tasks->get_latest_host_comms_message<
                                messages::GetTemperatureResponse>();
                        REQUIRE(query_response.setpoint_temperature == 0.125);
                    }
                }
                AND_WHEN("continuing to get thermistor readings") {
                    auto conversion_message =
                        messages::TemperatureConversionComplete{
                            .pad_a = ((1U << 9) - 1),
                            .pad_b = (1U << 9),
                            .board = (1U << 11)};
                    tasks->consume_heater_message(conversion_message);
                    REQUIRE(tasks->get_heater_policy().last_power_setting() ==
                            0.125);
                }
            }
        }
        WHEN("sending a set-temperature message as if from host comms") {
            auto message = messages::SetTemperatureMessage{
                .id = 1231, .target_temperature = _valid_temp};
            tasks->consume_heater_message(message);
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to host comms") {
                    tasks->require_has_ack_for(message);
                    AND_WHEN("sending a get-temperature query") {
                        auto gettemp =
                            messages::GetTemperatureMessage{.id = 1232};
                        tasks->consume_heater_message(gettemp);
                        THEN("the response should have the new setpoint") {
                            auto gettemp = tasks->get_latest_host_comms_message<
                                messages::GetTemperatureResponse>();
                            REQUIRE(gettemp.setpoint_temperature ==
                                    message.target_temperature);
                        }
                    }
                    AND_WHEN(
                        "sending a set-temperature with an out of range "
                        "value") {
                        auto message2 = messages::SetTemperatureMessage{
                            .id = 1233, .target_temperature = 105};
                        tasks->consume_heater_message(message2);
                        THEN(
                            "the response should indicate an out of range "
                            "error") {
                            tasks->require_has_ack_for(
                                message2,
                                errors::ErrorCode::
                                    HEATER_ILLEGAL_TARGET_TEMPERATURE);
                            AND_WHEN("sending a get-temperature query") {
                                auto gettemp =
                                    messages::GetTemperatureMessage{.id = 1234};
                                tasks->consume_heater_message(gettemp);
                                THEN(
                                    "the response should have the old "
                                    "setpoint") {
                                    auto gettempresponse =
                                        tasks->get_latest_host_comms_message<
                                            messages::GetTemperatureResponse>();
                                    REQUIRE(
                                        gettempresponse.setpoint_temperature ==
                                        message.target_temperature);
                                }
                            }
                        }
                    }
                    AND_WHEN("sending a deactivate-heater command") {
                        auto message2 =
                            messages::DeactivateHeaterMessage{.id = 1234};
                        tasks->consume_heater_message(message2);

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
                                tasks->require_has_ack_for(message2);
                                AND_WHEN(
                                    "sending a subsequent set-temperature "
                                    "command and valid ADC readings") {
                                    auto message3 =
                                        messages::SetTemperatureMessage{
                                            .id = 1235,
                                            .target_temperature = _valid_temp};
                                    tasks->consume_heater_message(message3);
                                    tasks->consume_heater_message(read_message);
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
                .id = 1234,
                .target_temperature = _valid_temp,
                .from_system = true};
            tasks->consume_heater_message(message);
            THEN("update-led-state-message should be produced") {
                REQUIRE(!tasks->get_system_queue().backing_deque.empty());
                auto response1 =
                    tasks->get_system_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::UpdateLEDStateMessage>(
                    response1));
                auto getresponse =
                    std::get<messages::UpdateLEDStateMessage>(response1);
                REQUIRE(getresponse.color == LED_COLOR::RED);
                REQUIRE(getresponse.mode == LED_MODE::SOLID_HOT);
                tasks->run_system_task();  // process UpdateLEDStateMessage
                AND_THEN(
                    "the task should get the message and respond to system") {
                    REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                    REQUIRE(!tasks->get_system_queue().backing_deque.empty());
                    auto response2 =
                        tasks->get_system_queue().backing_deque.front();
                    tasks->get_system_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response2));
                    auto ack =
                        std::get<messages::AcknowledgePrevious>(response2);
                    REQUIRE(ack.responding_to_id == message.id);
                }
            }
        }
        WHEN("sending a get-temperature message") {
            auto message = messages::GetTemperatureMessage{.id = 999};
            tasks->consume_heater_message(message);
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the message") {
                    auto gettemp = tasks->get_latest_host_comms_message<
                        messages::GetTemperatureResponse>();
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE(gettemp.setpoint_temperature == std::nullopt);
                    REQUIRE_THAT(
                        gettemp.current_temperature,
                        Catch::Matchers::WithinAbs(_valid_temp_offset, .01));
                }
            }
        }
        WHEN("sending a get-temperature-debug message") {
            auto message = messages::GetTemperatureDebugMessage{.id = 123};
            tasks->consume_heater_message(message);
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the message") {
                    auto gettemp = tasks->get_latest_host_comms_message<
                        messages::GetTemperatureDebugResponse>();
                    REQUIRE(gettemp.responding_to_id == message.id);
                    REQUIRE_THAT(
                        gettemp.pad_a_temperature,
                        Catch::Matchers::WithinAbs(_valid_temp_offset, 0.1));
                    REQUIRE_THAT(
                        gettemp.pad_b_temperature,
                        Catch::Matchers::WithinAbs(_valid_temp_offset, 0.1));
                    REQUIRE_THAT(gettemp.board_temperature,
                                 Catch::Matchers::WithinAbs(_valid_temp, 0.1));
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
            tasks->get_host_comms_queue().backing_deque.clear();
            tasks->consume_heater_message(offset_set_msg);
            // Send temperatures to refresh calculations
            tasks->consume_heater_message(read_message);
            THEN("the task should get the message") {
                REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                AND_THEN("the task should respond to the messsage") {
                    tasks->require_has_ack_for(offset_set_msg);
                }
            }
            AND_WHEN("sending a get-temperature-debug message") {
                auto message = messages::GetTemperatureDebugMessage{.id = 123};
                tasks->get_host_comms_queue().backing_deque.clear();
                tasks->consume_heater_message(message);
                THEN("the task should get the message") {
                    REQUIRE(tasks->get_heater_queue().backing_deque.empty());
                    AND_THEN(
                        "the temperature should be changed by the offset") {
                        double adjusted_temp = (2.0 * _valid_temp) + 6.0F;
                        auto gettemp = tasks->get_latest_host_comms_message<
                            messages::GetTemperatureDebugResponse>();
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
                tasks->get_host_comms_queue().backing_deque.clear();
                tasks->consume_heater_message(get_offsets);
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
        WHEN("sending a clear-error-state message") {
            auto message = messages::ClearErrorStateMessage{.id = 1231};
            tasks->consume_heater_message(message);
            THEN("the task should acknowledge") {
                tasks->require_has_ack_for(message);
            }
        }
    }

    GIVEN("a heater task with an invalid out-of-range temp") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = (1U << 9), .pad_b = 0, .board = (1U << 11)};
        tasks->consume_heater_message(read_message);
        CHECK(std::holds_alternative<messages::ErrorMessage>(
            tasks->get_host_comms_queue().backing_deque.front()));
        tasks->get_host_comms_queue().backing_deque.pop_front();
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
        WHEN("sending a set-temperature message") {
            auto message = messages::SetTemperatureMessage{
                .id = 1231, .target_temperature = 60};
            tasks->consume_heater_message(message);
            THEN("the task should respond appropriately") {
                tasks->require_has_ack_for(
                    message, errors::ErrorCode::HEATER_THERMISTOR_B_SHORT);
                REQUIRE(!tasks->get_system_queue().backing_deque.empty());
                auto response2 =
                    tasks->get_system_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::UpdateLEDStateMessage>(
                    response2));
                auto getresponse =
                    std::get<messages::UpdateLEDStateMessage>(response2);
                REQUIRE(getresponse.color == LED_COLOR::AMBER);
                REQUIRE(getresponse.mode == LED_MODE::PULSE);
            }
        }
        WHEN("sending a get-temperature message") {
            auto message = messages::GetTemperatureMessage{.id = 2222};
            tasks->consume_heater_message(message);
            THEN("the task should respond with an error") {
                tasks->require_has_ack_for<messages::GetTemperatureMessage,
                                           messages::GetTemperatureResponse>(
                    message, errors::ErrorCode::HEATER_THERMISTOR_B_SHORT);
            }
        }
    }
}

SCENARIO("heater task error handling") {
    GIVEN("a heater task with no errors") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = ((1U << 9) - 1), .pad_b = (1U << 9), .board = (1U << 11)};
        tasks->consume_heater_message(read_message);
        CHECK(tasks->get_host_comms_queue().backing_deque.empty());
        WHEN("setting thermistor a to an error state and setting the latch") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = 0, .pad_b = (1U << 9), .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->consume_heater_message(one_error_message);
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
            AND_WHEN("a get error message is received") {
                tasks->get_host_comms_queue().backing_deque.clear();
                auto get_error = messages::GetErrorStateMessage{.id = 1233};
                tasks->consume_heater_message(get_error);
                THEN("it should get the error state") {
                    tasks->require_has_ack_for(
                        get_error,
                        errors::ErrorCode::HEATER_THERMISTOR_A_SHORT);
                }
            }
        }
        WHEN("setting thermistor b to an error state and setting the latch") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = ((1U << 9) - 1), .pad_b = 0, .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->consume_heater_message(one_error_message);
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
            AND_WHEN("a get error message is received") {
                tasks->get_host_comms_queue().backing_deque.clear();
                auto get_error = messages::GetErrorStateMessage{.id = 1233};
                tasks->consume_heater_message(get_error);
                THEN("it should get the error state") {
                    tasks->require_has_ack_for(
                        get_error,
                        errors::ErrorCode::HEATER_THERMISTOR_B_SHORT);
                }
            }
        }
        WHEN(
            "setting both thermistors to an error state and setting the "
            "latch") {
            auto one_error_message = messages::TemperatureConversionComplete{
                .pad_a = 0, .pad_b = ((1U << 12) - 1), .board = (1U << 11)};
            tasks->get_heater_policy().set_power_good(false);
            tasks->consume_heater_message(one_error_message);
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
            AND_WHEN("a get error message is received") {
                tasks->get_host_comms_queue().backing_deque.clear();
                auto get_error = messages::GetErrorStateMessage{.id = 1233};
                tasks->consume_heater_message(get_error);
                THEN("it should get the error state") {
                    tasks->require_has_ack_for(
                        get_error,
                        errors::ErrorCode::HEATER_THERMISTOR_A_SHORT);
                }
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
            tasks->consume_heater_message(one_error_message);
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
                    tasks->consume_heater_message(one_error_message);
                    THEN("latch should reset") {
                        CHECK(tasks->get_host_comms_queue()
                                  .backing_deque.empty());
                        REQUIRE(
                            tasks->get_heater_policy().try_reset_call_count() ==
                            1);
                        REQUIRE(tasks->get_heater_policy().power_good());
                    }
                }
                AND_WHEN("a get error message is received") {
                    tasks->get_host_comms_queue().backing_deque.clear();
                    auto get_error = messages::GetErrorStateMessage{.id = 1233};
                    tasks->consume_heater_message(get_error);
                    THEN("it should get the error state") {
                        tasks->require_has_ack_for(
                            get_error, errors::ErrorCode::
                                           HEATER_THERMISTOR_A_DISCONNECTED);
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
            tasks->consume_heater_message(one_error_message);
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
                    tasks->consume_heater_message(one_error_message);
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
            tasks->consume_heater_message(read_message);
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
                tasks->consume_heater_message(set_temp_message);
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                tasks->require_has_ack_for(
                    set_temp_message,
                    errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
            }
        }
        WHEN("setting a circuit error") {
            auto message = messages::SetTemperatureMessage{
                .id = 1231,
                .target_temperature =
                    60};  // to put task into CONTROLLING state
            tasks->consume_heater_message(message);
            tasks->get_host_comms_queue().backing_deque.pop_front();
            tasks->get_heater_policy().set_circuit_error(true);
            tasks->consume_heater_message(read_message);
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
                        errors::ErrorCode::HEATER_HARDWARE_SHORT_CIRCUIT);
                auto set_temp_message = messages::SetTemperatureMessage{
                    .id = 24, .target_temperature = 29.2};
                tasks->consume_heater_message(set_temp_message);
                CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
                tasks->require_has_ack_for(
                    set_temp_message,
                    errors::ErrorCode::HEATER_HARDWARE_SHORT_CIRCUIT);
            }
        }
        WHEN("setting a custom error with no wait time") {
            auto message = messages::SetErrorStateMessage{
                .id = 1231,
                .error_to_set =
                    errors::ErrorCode::HEATER_HARDWARE_SHORT_CIRCUIT,
                .delay_s = 0};
            tasks->consume_heater_message(message);
            THEN("the task should acknowledge") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                tasks->require_has_ack_for(message);
            }
            THEN("after one more spin the task should be in error state") {
                auto valid_adc = _converter.backconvert(_valid_temp);
                auto read_message = messages::TemperatureConversionComplete{
                    .pad_a = valid_adc, .pad_b = valid_adc, .board = valid_adc};
                tasks->consume_heater_message(read_message);
                THEN("after one spin the task should be in error state") {
                    REQUIRE(tasks->get_heater_task().most_relevant_error() ==
                            errors::ErrorCode::HEATER_HARDWARE_SHORT_CIRCUIT);
                }
            }
        }
    }
    GIVEN("a heater task with a thermistor reading something bad") {
        auto tasks = TaskBuilder::build();
        auto read_message = messages::TemperatureConversionComplete{
            .pad_a = 0, .pad_b = (1U << 15), .board = (1U << 11)};
        tasks->get_heater_policy().set_power_good(false);
        tasks->consume_heater_message(read_message);
        CHECK(!tasks->get_host_comms_queue().backing_deque.empty());
        tasks->get_host_comms_queue().backing_deque.clear();
        WHEN("the error goes away and the latch is allowed to be reset") {
            read_message = messages::TemperatureConversionComplete{
                .pad_a = ((1U << 9) - 1),
                .pad_b = (1U << 9),
                .board = (1U << 11)};
            tasks->consume_heater_message(read_message);
            THEN("there is no error and the heater task works normally") {
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                auto settemp = messages::SetTemperatureMessage{
                    .id = 54, .target_temperature = 43};
                tasks->consume_heater_message(settemp);
                tasks->require_has_ack_for(settemp);
            }
        }
        WHEN("the pad error goes away but the latch cannot reset") {
            auto read_message = messages::TemperatureConversionComplete{
                .pad_a = ((1U << 9) - 1),
                .pad_b = (1U << 9),
                .board = (1U << 11)};
            tasks->get_heater_policy().set_can_reset(false);
            tasks->consume_heater_message(read_message);
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
                    tasks->consume_heater_message(settemp);
                    THEN("there is an error response") {
                        CHECK(!tasks->get_host_comms_queue()
                                   .backing_deque.empty());
                        tasks->require_has_ack_for(
                            settemp,
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
        tasks->get_heater_policy().set_power_good(false);
        tasks->get_heater_policy().set_can_reset(false);
        tasks->consume_heater_message(read_message);
        tasks->get_host_comms_queue().backing_deque.clear();
        WHEN("sending a set-temp with the latch allowed to reset") {
            tasks->get_heater_policy().set_can_reset(true);
            tasks->get_heater_policy().reset_try_reset_call_count();
            auto settemp = messages::SetTemperatureMessage{
                .id = 254, .target_temperature = 54};
            tasks->consume_heater_message(settemp);
            THEN("the set temp should reset the latch and succeed") {
                REQUIRE(tasks->get_heater_policy().try_reset_call_count() == 1);
                REQUIRE(tasks->get_heater_policy().power_good());
                tasks->require_has_ack_for(settemp);
            }
        }
        WHEN("sending a set-temp with the latch not allowed to reset") {
            tasks->get_heater_policy().set_can_reset(false);
            tasks->get_heater_policy().reset_try_reset_call_count();
            auto settemp = messages::SetTemperatureMessage{
                .id = 254, .target_temperature = 54};
            tasks->consume_heater_message(settemp);
            THEN(
                "the set temp should try and fail to reset the latch and send "
                "an error") {
                REQUIRE(tasks->get_heater_policy().try_reset_call_count() == 1);
                tasks->require_has_ack_for(
                    settemp, errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
            }
        }
        WHEN("sending a clear-error-state with the latch allowed to reset") {
            tasks->get_heater_policy().set_can_reset(true);
            tasks->get_heater_policy().reset_try_reset_call_count();
            auto clearmessage = messages::ClearErrorStateMessage{.id = 254};
            tasks->consume_heater_message(clearmessage);
            THEN("the set temp should reset the latch and succeed") {
                REQUIRE(tasks->get_heater_policy().try_reset_call_count() == 1);
                REQUIRE(tasks->get_heater_policy().power_good());
                tasks->require_has_ack_for(clearmessage);
            }
        }
        WHEN(
            "sending a clear-error-state with the latch not allowed to reset") {
            tasks->get_heater_policy().set_can_reset(false);
            tasks->get_heater_policy().reset_try_reset_call_count();
            auto clearmessage = messages::ClearErrorStateMessage{.id = 254};
            tasks->consume_heater_message(clearmessage);
            THEN("the set temp should reset the latch and succeed") {
                REQUIRE(tasks->get_heater_policy().try_reset_call_count() == 1);
                tasks->require_has_ack_for(
                    clearmessage,
                    errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH);
            }
        }
    }
}
