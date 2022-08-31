#include "catch2/catch.hpp"
#include "test/test_tasks.hpp"
#include "test/test_thermal_policy.hpp"

TEST_CASE("thermal task message handling") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;
    thermistor_conversion::Conversion<lookups::NXFT15XV103FA2B030> converter(
        decltype(tasks->_thermal_task)::THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
        decltype(tasks->_thermal_task)::ADC_BIT_MAX, false);
    WHEN("new thermistor readings are received") {
        auto plate_count = converter.backconvert(25.00);
        auto hs_count = converter.backconvert(50.00);
        auto thermistors_msg = messages::ThermistorReadings{
            .timestamp = 1000,
            .plate = plate_count,
            .heatsink = hs_count,
        };
        tasks->_thermal_queue.backing_deque.push_back(thermistors_msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the message is consumed") {
            REQUIRE(!tasks->_thermal_queue.has_message());
        }
        THEN("the readings are updated properly") {
            auto readings = tasks->_thermal_task.get_readings();
            REQUIRE(readings.heatsink_adc == thermistors_msg.heatsink);
            REQUIRE(readings.plate_adc == thermistors_msg.plate);
            REQUIRE(readings.last_tick == thermistors_msg.timestamp);
        }
        THEN("the ADC readings are properly converted to temperatures") {
            auto readings = tasks->_thermal_task.get_readings();
            REQUIRE(readings.plate_temp.has_value());
            REQUIRE(readings.heatsink_temp.has_value());
            REQUIRE_THAT(readings.plate_temp.value(),
                         Catch::Matchers::WithinAbs(25.00, 0.01));
            REQUIRE_THAT(readings.heatsink_temp.value(),
                         Catch::Matchers::WithinAbs(50.00, 0.01));
        }
        AND_WHEN("a GetTempDebug message is received") {
            tasks->_thermal_queue.backing_deque.push_back(
                messages::GetTempDebugMessage{.id = 123});
            tasks->_thermal_task.run_once(policy);
            THEN("the message is consumed") {
                REQUIRE(!tasks->_thermal_queue.has_message());
            }
            THEN("a response is sent to the comms task with the correct data") {
                REQUIRE(tasks->_comms_queue.has_message());
                REQUIRE(std::holds_alternative<messages::GetTempDebugResponse>(
                    tasks->_comms_queue.backing_deque.front()));
                auto response = std::get<messages::GetTempDebugResponse>(
                    tasks->_comms_queue.backing_deque.front());
                REQUIRE(response.responding_to_id == 123);
                REQUIRE_THAT(response.plate_temp,
                             Catch::Matchers::WithinAbs(25.00, 0.01));
                REQUIRE_THAT(response.heatsink_temp,
                             Catch::Matchers::WithinAbs(50.00, 0.01));
                REQUIRE(response.plate_adc == plate_count);
                REQUIRE(response.heatsink_adc == hs_count);
            }
        }
    }
}

TEST_CASE("thermal task SetPeltierDebug functionality") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;
    REQUIRE(!policy._enabled);
    WHEN("setting peltiers to heat") {
        auto msg = messages::SetPeltierDebugMessage{.id = 123, .power = 0.5};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the peltiers are enabled in heat mode") {
            REQUIRE(policy._enabled);
            REQUIRE(policy._power == msg.power);
            REQUIRE(policy.is_heating());
            REQUIRE(tasks->_thermal_task.get_peltier().manual);
            REQUIRE(tasks->_thermal_task.get_peltier().power == msg.power);
        }
        THEN("the task sends an ack to the comms task") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
        }
    }
    WHEN("setting peltiers to cool") {
        auto msg = messages::SetPeltierDebugMessage{.id = 123, .power = -0.5};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the peltiers are enabled in cooling mode") {
            REQUIRE(policy._enabled);
            REQUIRE(policy._power == msg.power);
            REQUIRE(policy.is_cooling());
            REQUIRE(tasks->_thermal_task.get_peltier().manual);
            REQUIRE(tasks->_thermal_task.get_peltier().power == msg.power);
        }
        THEN("the task sends an ack to the comms task") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
        }
        AND_THEN("disabling the peltiers") {
            msg.id = 456;
            msg.power = 0;
            tasks->_thermal_queue.backing_deque.push_back(msg);
            tasks->_thermal_task.run_once(policy);
            THEN("the peltiers are disabled") { REQUIRE(!policy._enabled); }
        }
    }
    WHEN("setting peltiers to heat above 100%% power") {
        auto msg = messages::SetPeltierDebugMessage{.id = 123, .power = 5};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the task does not enable the peltier") {
            REQUIRE(!policy._enabled);
        }
        THEN("the task sends an error to the comms task") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error ==
                    errors::ErrorCode::THERMAL_PELTIER_POWER_ERROR);
        }
    }
    WHEN("setting peltiers to cool below 100%% power") {
        auto msg = messages::SetPeltierDebugMessage{.id = 123, .power = -5};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the task does not enable the peltier") {
            REQUIRE(!policy._enabled);
        }
        THEN("the task sends an error to the comms task") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error ==
                    errors::ErrorCode::THERMAL_PELTIER_POWER_ERROR);
        }
    }
}

TEST_CASE("thermal task fan command functionality") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;
    REQUIRE(!policy._enabled);
    WHEN("setting fans to manual power") {
        // Preload fans with power to test that they get reset
        policy._fans = -1.0F;

        auto power = GENERATE(0.0F, 0.1F, 0.5F, 1.0F);

        auto msg = messages::SetFanManualMessage{.id = 123, .power = power};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the task sets the fan power accordingly") {
            REQUIRE(policy._fans == power);
        }
        THEN("the fans are set to manual mode") {
            REQUIRE(!tasks->_thermal_queue.has_message());
            REQUIRE(tasks->_thermal_task.get_fan().manual);
        }
        THEN("an ack is sent to host comms task") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
        }
        AND_WHEN("setting fans to automatic mode") {
            tasks->_comms_queue.backing_deque.clear();
            auto auto_msg = messages::SetFanAutomaticMessage{.id = 456};
            tasks->_thermal_queue.backing_deque.push_back(auto_msg);
            tasks->_thermal_task.run_once(policy);
            THEN("the message is consumed and the fans are set to automatic") {
                REQUIRE(!tasks->_thermal_queue.has_message());
                REQUIRE(!tasks->_thermal_task.get_fan().manual);
            }
            THEN("an ack is sent to host comms task") {
                REQUIRE(tasks->_comms_queue.has_message());
                auto host_msg = tasks->_comms_queue.backing_deque.front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    host_msg));
                auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
                REQUIRE(ack.responding_to_id == auto_msg.id);
                REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
            }
        }
    }
}

TEST_CASE("pid setting command handling") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;
    WHEN("sending new PID constants") {
        auto msg =
            messages::SetPIDConstantsMessage{.id = 123, .p = 1, .i = 2, .d = 3};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the pid constants are updated in the task") {
            auto pid = tasks->_thermal_task.get_pid();
            REQUIRE(pid.kp() == msg.p);
            REQUIRE(pid.ki() == msg.i);
            REQUIRE(pid.kd() == msg.d);
        }
        THEN("the task sends an ack to Host Comms") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
        }
    }
    WHEN("sending out-of-bounds PID constants") {
        auto msg = messages::SetPIDConstantsMessage{
            .id = 123, .p = 2000, .i = -4000, .d = 3};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the pid constants are clamped by the task") {
            auto pid = tasks->_thermal_task.get_pid();
            REQUIRE(pid.kp() == tasks->_thermal_task.PELTIER_K_MAX);
            REQUIRE(pid.ki() == tasks->_thermal_task.PELTIER_K_MIN);
            REQUIRE(pid.kd() == msg.d);
        }
        THEN("the task sends an ack to Host Comms") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
        }
    }
}

TEST_CASE("deactivation command") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;

    GIVEN("the thermal task is in manual fan + peltier mode") {
        tasks->_thermal_queue.backing_deque.push_back(
            messages::SetFanManualMessage{.id = 123, .power = 0.5});
        tasks->_thermal_queue.backing_deque.push_back(
            messages::SetPeltierDebugMessage{.id = 456, .power = 0.5});
        tasks->_thermal_task.run_once(policy);
        tasks->_thermal_task.run_once(policy);
        REQUIRE_THAT(policy._fans, Catch::Matchers::WithinAbs(0.5, 0.001));
        REQUIRE_THAT(policy._power, Catch::Matchers::WithinAbs(0.5, 0.001));
        REQUIRE(policy._enabled);
        WHEN("sending a deactivation command") {
            tasks->_comms_queue.backing_deque.clear();
            auto msg = messages::DeactivateAllMessage{.id = 999};
            tasks->_thermal_queue.backing_deque.push_back(msg);
            tasks->_thermal_task.run_once(policy);
            THEN("fans and peltier are deactivated") {
                REQUIRE(policy._fans == 0.0F);
                REQUIRE(policy._power == 0.0F);
                REQUIRE(!policy._enabled);
            }
            THEN("the task sends an ack to Host Comms") {
                REQUIRE(tasks->_comms_queue.has_message());
                auto host_msg = tasks->_comms_queue.backing_deque.front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    host_msg));
                auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
                REQUIRE(ack.responding_to_id == msg.id);
                REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
            }
        }
    }
}

TEST_CASE("thermal task set temperature command") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;

    REQUIRE(!tasks->_thermal_task.get_peltier().target_set);
    WHEN("setting a target temperature") {
        auto msg = messages::SetTemperatureMessage{.id = 123, .target = 100};
        tasks->_thermal_queue.backing_deque.push_back(msg);
        tasks->_thermal_task.run_once(policy);
        THEN("the task updates its target") {
            REQUIRE(!tasks->_thermal_task.get_peltier().manual);
            REQUIRE(tasks->_thermal_task.get_peltier().target_set);
            REQUIRE(tasks->_thermal_task.get_peltier().target == msg.target);
        }
        THEN("the task sends an ack to Host Comms") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
            REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
        }
        AND_WHEN("requesting manual peltiers mode") {
            tasks->_comms_queue.backing_deque.clear();
            auto manual_msg =
                messages::SetPeltierDebugMessage{.id = 555, .power = 1};
            tasks->_thermal_queue.backing_deque.push_back(manual_msg);
            tasks->_thermal_task.run_once(policy);
            THEN("the task does NOT change to manual mode") {
                REQUIRE(!tasks->_thermal_task.get_peltier().manual);
                REQUIRE(tasks->_thermal_task.get_peltier().target_set);
                REQUIRE(tasks->_thermal_task.get_peltier().target ==
                        msg.target);
            }
            THEN("the task sends an error to Host Comms") {
                REQUIRE(tasks->_comms_queue.has_message());
                auto host_msg = tasks->_comms_queue.backing_deque.front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    host_msg));
                auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
                REQUIRE(ack.responding_to_id == manual_msg.id);
                REQUIRE(ack.with_error ==
                        errors::ErrorCode::THERMAL_PELTIER_BUSY);
            }
        }
    }
    GIVEN("the peltiers are in manual mode") {
        auto manual_msg =
            messages::SetPeltierDebugMessage{.id = 555, .power = 1};
        tasks->_thermal_queue.backing_deque.push_back(manual_msg);
        tasks->_thermal_task.run_once(policy);
        REQUIRE(tasks->_thermal_task.get_peltier().manual);
        REQUIRE(!tasks->_thermal_task.get_peltier().target_set);
        REQUIRE(policy._power == 1.0F);
        WHEN("setting a target temperature") {
            tasks->_comms_queue.backing_deque.clear();
            auto msg =
                messages::SetTemperatureMessage{.id = 777, .target = 100};
            tasks->_thermal_queue.backing_deque.push_back(msg);
            tasks->_thermal_task.run_once(policy);
            THEN("the task updates its target") {
                REQUIRE(!tasks->_thermal_task.get_peltier().manual);
                REQUIRE(tasks->_thermal_task.get_peltier().target_set);
                REQUIRE(tasks->_thermal_task.get_peltier().target ==
                        msg.target);
            }
            THEN("the peltier power is not cleared out immediately") {
                REQUIRE(policy._power == 1.0F);
                REQUIRE(policy._enabled);
            }
            THEN("the task sends an ack to Host Comms") {
                REQUIRE(tasks->_comms_queue.has_message());
                auto host_msg = tasks->_comms_queue.backing_deque.front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    host_msg));
                auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
                REQUIRE(ack.responding_to_id == msg.id);
                REQUIRE(ack.with_error == errors::ErrorCode::NO_ERROR);
            }
        }
    }
}

TEST_CASE("closed loop thermal control") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;
    thermistor_conversion::Conversion<lookups::NXFT15XV103FA2B030> converter(
        decltype(tasks->_thermal_task)::THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM,
        decltype(tasks->_thermal_task)::ADC_BIT_MAX, false);

    uint32_t timestamp_increment = 100;
    auto temp_message =
        messages::ThermistorReadings{.timestamp = timestamp_increment,
                                     .plate = converter.backconvert(25),
                                     .heatsink = converter.backconvert(25)};
    tasks->_thermal_queue.backing_deque.push_back(temp_message);
    tasks->_thermal_task.run_once(policy);
    GIVEN("ambient temperature") {
        THEN("the fan is set to OFF") {
            REQUIRE_THAT(policy._fans, Catch::Matchers::WithinAbs(0, 0.001));
        }
    }
    GIVEN("high temperature when at rest") {
        temp_message.heatsink = converter.backconvert(60);
        temp_message.timestamp += timestamp_increment;
        tasks->_thermal_queue.backing_deque.push_back(temp_message);
        tasks->_thermal_task.run_once(policy);

        THEN("the fan is set on") {
            REQUIRE_THAT(policy._fans,
                         Catch::Matchers::WithinAbs(
                             tasks->_thermal_task.FAN_POWER_LOW, 0.001));
        }
    }
    WHEN("setting temp target to 100ºC") {
        auto target_msg =
            messages::SetTemperatureMessage{.id = 123, .target = 100};
        tasks->_thermal_queue.backing_deque.push_back(target_msg);
        tasks->_thermal_task.run_once(policy);
        AND_WHEN("temperature readings are updated") {
            temp_message.timestamp += timestamp_increment;
            tasks->_thermal_queue.backing_deque.push_back(temp_message);
            tasks->_thermal_task.run_once(policy);
            THEN("the peltiers update to heat") {
                REQUIRE(policy._enabled);
                REQUIRE(policy.is_heating());
            }
            THEN("the fan is set to MEDIUM") {
                REQUIRE_THAT(policy._fans,
                             Catch::Matchers::WithinAbs(
                                 tasks->_thermal_task.FAN_POWER_MEDIUM, 0.001));
            }
            THEN("the PID sampletime is correct") {
                auto expected = 0.001 * timestamp_increment;
                REQUIRE_THAT(tasks->_thermal_task.get_pid().sampletime(),
                             Catch::Matchers::WithinAbs(expected, 0.0001));
            }
        }
    }
    WHEN("setting temp target to -4ºC") {
        auto target_msg =
            messages::SetTemperatureMessage{.id = 123, .target = -4};
        tasks->_thermal_queue.backing_deque.push_back(target_msg);
        tasks->_thermal_task.run_once(policy);
        AND_WHEN("temperature readings are updated") {
            temp_message.timestamp += timestamp_increment;
            tasks->_thermal_queue.backing_deque.push_back(temp_message);
            tasks->_thermal_task.run_once(policy);
            THEN("the peltiers update to cool") {
                REQUIRE(policy._enabled);
                REQUIRE(policy.is_cooling());
            }
            THEN("the fan is set to HIGH") {
                REQUIRE_THAT(policy._fans,
                             Catch::Matchers::WithinAbs(
                                 tasks->_thermal_task.FAN_POWER_MAX, 0.001));
            }
        }
    }
}

TEST_CASE("thermal task offset constants message handling") {
    auto *tasks = tasks::BuildTasks();
    TestThermalPolicy policy;

    WHEN("getting the offset constants") {
        auto get_msg = messages::GetOffsetConstantsMessage{.id = 1};
        REQUIRE(tasks->_thermal_queue.try_send(get_msg));
        tasks->_thermal_task.run_once(policy);
        THEN("the thermal task responds with the default constants") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto response = tasks->_comms_queue.backing_deque.front();
            REQUIRE(
                std::holds_alternative<messages::GetOffsetConstantsResponse>(
                    response));
            auto response_msg =
                std::get<messages::GetOffsetConstantsResponse>(response);
            REQUIRE(response_msg.responding_to_id == get_msg.id);
            REQUIRE(response_msg.a ==
                    tasks->_thermal_task.OFFSET_DEFAULT_CONST_A);
            REQUIRE(response_msg.b ==
                    tasks->_thermal_task.OFFSET_DEFAULT_CONST_B);
            REQUIRE(response_msg.c ==
                    tasks->_thermal_task.OFFSET_DEFAULT_CONST_C);
        }
    }
    WHEN("setting B and C constants") {
        auto set_msg = messages::SetOffsetConstantsMessage{
            .id = 456, .a = std::nullopt, .b = 1, .c = 2};
        REQUIRE(tasks->_thermal_queue.try_send(set_msg));
        tasks->_thermal_task.run_once(policy);
        THEN("the thermal task responds with an ack") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto response = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                response));
            auto response_msg =
                std::get<messages::AcknowledgePrevious>(response);
            REQUIRE(response_msg.responding_to_id == set_msg.id);
        }
        AND_THEN("getting the offset constants") {
            tasks->_comms_queue.backing_deque.clear();
            auto get_msg = messages::GetOffsetConstantsMessage{.id = 1};
            REQUIRE(tasks->_thermal_queue.try_send(get_msg));
            tasks->_thermal_task.run_once(policy);
            THEN("the thermal task responds with the default constants") {
                REQUIRE(tasks->_comms_queue.has_message());
                auto response = tasks->_comms_queue.backing_deque.front();
                REQUIRE(std::holds_alternative<
                        messages::GetOffsetConstantsResponse>(response));
                auto response_msg =
                    std::get<messages::GetOffsetConstantsResponse>(response);
                REQUIRE(response_msg.responding_to_id == get_msg.id);
                REQUIRE(response_msg.a ==
                        tasks->_thermal_task.OFFSET_DEFAULT_CONST_A);
                REQUIRE(response_msg.b == set_msg.b.value());
                REQUIRE(response_msg.c == set_msg.c.value());
            }
        }
    }
}
