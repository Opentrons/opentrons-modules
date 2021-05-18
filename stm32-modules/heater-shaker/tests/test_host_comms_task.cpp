#include <cstring>
#include <string>

#include "catch2/catch.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/messages.hpp"
#include "test/task_builder.hpp"

SCENARIO("usb message parsing") {
    GIVEN("a host_comms_task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
        WHEN("calling run_once() with nothing in the queue") {
            THEN("the task should call recv(), which should throw") {
                REQUIRE_THROWS(tasks->get_host_comms_task().run_once(
                    tx_buf.begin(), tx_buf.end()));
            }
        }
        WHEN("calling run_once() with an empty gcode message") {
            auto message_text = std::string("\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            THEN("the task should call recv() and get the message") {
                REQUIRE_NOTHROW(tasks->get_host_comms_task().run_once(
                    tx_buf.begin(), tx_buf.end()));
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
            }
            THEN("the task writes nothing to the transmit buffer") {
                auto written = tasks->get_host_comms_task().run_once(
                    tx_buf.begin(), tx_buf.end());
                REQUIRE(written == tx_buf.begin());
                REQUIRE_THAT(tx_buf,
                             Catch::Matchers::Equals(std::string(128, 'c')));
            }
        }
        WHEN(
            "calling run_once() with an insufficient tx buffer when it wants "
            "to write data") {
            auto message_text = std::string("aslkdhasd\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            std::string small_buf(64, 'c');
            auto ends_at =
                errors::write_into(small_buf.begin(), small_buf.end(),
                                   errors::ErrorCode::USB_TX_OVERRUN);
            small_buf.resize(ends_at - small_buf.begin() - 5);
            auto written = tasks->get_host_comms_task().run_once(
                small_buf.begin(), small_buf.end());
            REQUIRE_THAT(small_buf,
                         Catch::Matchers::Equals("ERR001:tx buffer ove"));
            REQUIRE(written ==
                    small_buf.begin() + strlen("ERR001:tx buffer ove"));
        }
        WHEN("calling run_once() with a malformed gcode message") {
            auto message_text = std::string("aosjhdakljshd\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            THEN("the task writes an error to the transmit buffer") {
                auto written = tasks->get_host_comms_task().run_once(
                    tx_buf.begin(), tx_buf.end());
                REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                         "ERR003:unhandled gcode\n"));
                REQUIRE(written ==
                        tx_buf.begin() + strlen("ERR003:unhandled gcode\n"));
            }
        }
    }
}

SCENARIO("message passing for ack-only gcodes from usb input") {
    GIVEN("a host_comms task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
        WHEN("sending a set-temp") {
            auto message_text = std::string("M104 S100\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the heater and not "
                "immediately ack") {
                REQUIRE(tasks->get_heater_queue().backing_deque.size() != 0);
                auto heater_message =
                    tasks->get_heater_queue().backing_deque.front();
                auto set_temp_message =
                    std::get<messages::SetTemperatureMessage>(heater_message);
                tasks->get_heater_queue().backing_deque.pop_front();
                REQUIRE(set_temp_message.target_temperature == 100);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          set_temp_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M104 OK\n"));
                        REQUIRE(written_secondpass ==
                                tx_buf.begin() + strlen("M104 OK\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_temp_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_temp_message.id,
                            .with_error =
                                errors::ErrorCode::MOTOR_UNKNOWN_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "ERR110:main motor:unknown error\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a set-rpm") {
            auto message_text = std::string("M3 S3000\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the motor and not "
                "immediately ack") {
                REQUIRE(tasks->get_motor_queue().backing_deque.size() != 0);
                auto motor_message =
                    tasks->get_motor_queue().backing_deque.front();
                auto set_rpm_message =
                    std::get<messages::SetRPMMessage>(motor_message);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(set_rpm_message.target_rpm == 3000);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          set_rpm_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M3 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_rpm_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_rpm_message.id,
                            .with_error =
                                errors::ErrorCode::MOTOR_UNKNOWN_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "ERR110:main motor:unknown error\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }

        WHEN("sending a set-acceleration") {
            auto message_text = std::string("M204 S3000\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the motor and not "
                "immediately ack") {
                REQUIRE(tasks->get_motor_queue().backing_deque.size() != 0);
                auto motor_message =
                    tasks->get_motor_queue().backing_deque.front();
                auto set_accel_message =
                    std::get<messages::SetAccelerationMessage>(motor_message);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(set_accel_message.rpm_per_s == 3000);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_accel_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M204 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_accel_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }

                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_accel_message.id,
                            .with_error =
                                errors::ErrorCode::MOTOR_UNKNOWN_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "ERR110:main motor:unknown error\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
    }
}

SCENARIO("message passing for response-carrying gcodes from usb input") {
    GIVEN("a host_comms task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
        WHEN("sending a get-temp") {
            auto message_text = std::string("M105\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the heater and not "
                "immediately ack") {
                REQUIRE(tasks->get_heater_queue().backing_deque.size() != 0);
                auto heater_message =
                    tasks->get_heater_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::GetTemperatureMessage>(
                    heater_message));
                auto get_temp_message =
                    std::get<messages::GetTemperatureMessage>(heater_message);
                tasks->get_heater_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetTemperatureResponse{
                            .responding_to_id = get_temp_message.id,
                            .current_temperature = 47,
                            .setpoint_temperature = 0});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should respond to the get-temp message") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "M105 C47 T0 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 15);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetTemperatureResponse{
                            .responding_to_id = get_temp_message.id + 1,
                            .current_temperature = 99,
                            .setpoint_temperature = 20});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong message type back to the "
                    "comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          get_temp_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a response with an error to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetTemperatureResponse{
                            .responding_to_id = get_temp_message.id,
                            .current_temperature = 99,
                            .setpoint_temperature = 15,
                            .with_error =
                                errors::ErrorCode::HEATER_THERMISTOR_B_SHORT});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);

                    tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                          tx_buf.end());
                    THEN(
                        "the task should write both the error and the "
                        "response") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "ERR206:heater:thermistor b short\n"));
                    }
                }
            }
        }
        WHEN("sending a get-rpm") {
            auto message_text = std::string("M123\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the motor and not "
                "immediately ack") {
                REQUIRE(tasks->get_motor_queue().backing_deque.size() != 0);
                auto motor_message =
                    tasks->get_motor_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::GetRPMMessage>(
                    motor_message));
                auto get_rpm_message =
                    std::get<messages::GetRPMMessage>(motor_message);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response =
                        messages::HostCommsMessage(messages::GetRPMResponse{
                            .responding_to_id = get_rpm_message.id,
                            .current_rpm = 1500,
                            .setpoint_rpm = 1750});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "M123 C1500 T1750 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 20);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response =
                        messages::HostCommsMessage(messages::GetRPMResponse{
                            .responding_to_id = get_rpm_message.id + 1,
                            .current_rpm = 9999,
                            .setpoint_rpm = 1590});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong message type back to the "
                    "comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          get_rpm_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }
    }
}

SCENARIO("message handling for other-task-initiated error communication") {
    GIVEN("a host_comms task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
        WHEN("sending an error as from the motor task") {
            auto message_obj = messages::HostCommsMessage(
                messages::ErrorMessage(errors::ErrorCode::MOTOR_ILLEGAL_SPEED));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written = tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                                 tx_buf.end());
            THEN("the task should write out the error") {
                REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                         "ERR120:main motor:illegal speed\n"));
                REQUIRE(*written == 'c');
            }
        }
    }
}
