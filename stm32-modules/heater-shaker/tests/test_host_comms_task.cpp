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
                    tx_buf.data(), tx_buf.size()));
            }
        }
        WHEN("calling run_once() with an empty gcode message") {
            auto message_text = std::string("\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    message_text.data(), message_text.size()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            THEN("the task should call recv() and get the message") {
                REQUIRE_NOTHROW(tasks->get_host_comms_task().run_once(
                    tx_buf.data(), tx_buf.size()));
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
            }
            THEN("the task writes nothing to the transmit buffer") {
                auto written = tasks->get_host_comms_task().run_once(
                    tx_buf.data(), tx_buf.size());
                REQUIRE(written == 0);
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
                    message_text.data(), message_text.size()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            std::string small_buf(errors::USBTXBufOverrun::errorstring());
            auto written = tasks->get_host_comms_task().run_once(
                small_buf.data(), small_buf.size());
            REQUIRE_THAT(small_buf,
                         Catch::Matchers::Equals(
                             errors::USBTXBufOverrun::errorstring()));
            REQUIRE(written == strlen(errors::USBTXBufOverrun::errorstring()));
        }
        WHEN("calling run_once() with a malformed gcode message") {
            auto message_text = std::string("aosjhdakljshd\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    message_text.data(), message_text.size()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            THEN("the task writes an error to the transmit buffer") {
                auto written = tasks->get_host_comms_task().run_once(
                    tx_buf.data(), tx_buf.size());
                REQUIRE(written ==
                        strlen(errors::UnhandledGCode::errorstring()));
                REQUIRE_THAT(tx_buf,
                             Catch::Matchers::StartsWith(
                                 errors::UnhandledGCode::errorstring()));
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
                    message_text.data(), message_text.size()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.data(), tx_buf.size());
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
                REQUIRE(written_firstpass == 0);
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          set_temp_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M104 OK\n"));
                        REQUIRE(written_secondpass == 8);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > 0);
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }
        WHEN("sending a set-rpm") {
            auto message_text = std::string("M3 S3000\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    message_text.data(), message_text.size()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.data(), tx_buf.size());
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
                REQUIRE(written_firstpass == 0);
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          set_rpm_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M3 OK\n"));
                        REQUIRE(written_secondpass == 6);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > 0);
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

SCENARIO("message passing for response-carrying gcodes from usb input") {
    GIVEN("a host_comms task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
        WHEN("sending a get-temp") {
            auto message_text = std::string("M105\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    message_text.data(), message_text.size()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.data(), tx_buf.size());
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
                REQUIRE(written_firstpass == 0);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN("the task should respond to the get-temp message") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "M105 C47 T0 OK\n"));
                        REQUIRE(written_secondpass == 15);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > 0);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > 0);
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }
        WHEN("sending a get-rpm") {
            auto message_text = std::string("M123\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    message_text.data(), message_text.size()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.data(), tx_buf.size());
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
                REQUIRE(written_firstpass == 0);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "M123 C1500 T1750 OK\n"));
                        REQUIRE(written_secondpass == 20);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > 0);
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
                        tasks->get_host_comms_task().run_once(tx_buf.data(),
                                                              tx_buf.size());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > 0);
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
