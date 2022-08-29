#include <cstring>
#include <string>

#include "catch2/catch.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/messages.hpp"
#include "systemwide.h"
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
                         Catch::Matchers::Equals(
                             "gcode response ERR001:tx buffer overru"));
            REQUIRE(written ==
                    small_buf.begin() +
                        strlen("gcode response ERR001:tx buffer overru"));
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
                REQUIRE_THAT(tx_buf,
                             Catch::Matchers::StartsWith(
                                 "gcode response ERR003:unhandled gcode OK\n"));
                REQUIRE(
                    written ==
                    tx_buf.begin() +
                        strlen("gcode response ERR003:unhandled gcode OK\n"));
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
                REQUIRE(!set_temp_message.from_system);
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR110:main "
                                                 "motor:unknown error OK\n"));
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
                REQUIRE(!set_rpm_message.from_system);
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR110:main "
                                                 "motor:unknown error OK\n"));
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR110:main "
                                                 "motor:unknown error OK\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }

        WHEN("sending a set-serial-number") {
            auto message_text = std::string("M996 TESTSN2xxxxxxxxxxxxxxxx\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the system and not "
                "immediately ack") {
                REQUIRE(tasks->get_system_queue().backing_deque.size() != 0);
                auto system_message =
                    tasks->get_system_queue().backing_deque.front();
                auto set_serial_number_message =
                    std::get<messages::SetSerialNumberMessage>(system_message);
                tasks->get_system_queue().backing_deque.pop_front();
                std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH> Test_SN = {
                    "TESTSN2xxxxxxxxxxxxxxxx"};
                REQUIRE(set_serial_number_message.serial_number == Test_SN);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_serial_number_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M996 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id =
                                set_serial_number_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_serial_number_message.id,
                            .with_error = errors::ErrorCode::
                                SYSTEM_SERIAL_NUMBER_HAL_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(
                            tx_buf,
                            Catch::Matchers::StartsWith(
                                "gcode response ERR302:system:HAL error, "
                                "busy, or timeout OK\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }

        WHEN("sending an open-platelock") {
            auto message_text = std::string("M242\n");
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
                auto open_platelock_message =
                    std::get<messages::OpenPlateLockMessage>(motor_message);
                tasks->get_motor_queue().backing_deque.pop_front();
                // REQUIRE(set_accel_message.rpm_per_s == 3000);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = open_platelock_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M242 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = open_platelock_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }

                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = open_platelock_message.id,
                            .with_error = errors::ErrorCode::MOTOR_NOT_HOME});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "gcode response ERR123:main motor:not "
                                         "home (required) OK\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }

        WHEN("sending a set-led-debug") {
            auto message_text = std::string("M994.D 0\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the system and not "
                "immediately ack") {
                REQUIRE(tasks->get_system_queue().backing_deque.size() != 0);
                auto system_message =
                    tasks->get_system_queue().backing_deque.front();
                auto set_led_message =
                    std::get<messages::SetLEDMessage>(system_message);
                tasks->get_system_queue().backing_deque.pop_front();
                REQUIRE(set_led_message.color == WHITE);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          set_led_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(
                            tx_buf, Catch::Matchers::StartsWith("M994.D OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_led_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_led_message.id,
                            .with_error =
                                errors::ErrorCode::SYSTEM_LED_TRANSMIT_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(
                            tx_buf,
                            Catch::Matchers::StartsWith(
                                "gcode response ERR304:system:LED I2C "
                                "transmission or "
                                "FreeRTOS notification passing failed OK\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending an identify-module-start-led") {
            auto message_text = std::string("M994\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the system and not "
                "immediately ack") {
                REQUIRE(tasks->get_system_queue().backing_deque.size() != 0);
                auto system_message =
                    tasks->get_system_queue().backing_deque.front();
                auto set_led_message =
                    std::get<messages::IdentifyModuleStartLEDMessage>(
                        system_message);
                tasks->get_system_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          set_led_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M994 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_led_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_led_message.id,
                            .with_error =
                                errors::ErrorCode::SYSTEM_LED_TRANSMIT_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(
                            tx_buf,
                            Catch::Matchers::StartsWith(
                                "gcode response ERR304:system:LED I2C "
                                "transmission or "
                                "FreeRTOS notification passing failed OK\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending an identify-module-stop-led") {
            auto message_text = std::string("M995\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the system and not "
                "immediately ack") {
                REQUIRE(tasks->get_system_queue().backing_deque.size() != 0);
                auto system_message =
                    tasks->get_system_queue().backing_deque.front();
                auto set_led_message =
                    std::get<messages::IdentifyModuleStopLEDMessage>(
                        system_message);
                tasks->get_system_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          set_led_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M995 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_led_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_led_message.id,
                            .with_error =
                                errors::ErrorCode::SYSTEM_LED_TRANSMIT_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(
                            tx_buf,
                            Catch::Matchers::StartsWith(
                                "gcode response ERR304:system:LED I2C "
                                "transmission or "
                                "FreeRTOS notification passing failed OK\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetOffsetConstants message") {
            auto message_text = std::string("M116\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass on the message and not immediately ack") {
                REQUIRE(tasks->get_heater_queue().has_message());
                auto heater_msg =
                    tasks->get_heater_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::SetOffsetConstantsMessage>(
                        heater_msg));
                auto message =
                    std::get<messages::SetOffsetConstantsMessage>(heater_msg);
                REQUIRE(!tasks->get_host_comms_queue().has_message());
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(!message.b_set);
                REQUIRE(!message.c_set);
                AND_WHEN("sending good response back") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        auto response = "M116 OK\n";
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(response));
                        REQUIRE(written_secondpass ==
                                tx_buf.begin() + strlen(response));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending invalid ID back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }

        WHEN("sending a deactivate heater") {
            auto message_text = std::string("M106\n");
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
                auto deactivate_heater_message =
                    std::get<messages::DeactivateHeaterMessage>(heater_message);
                tasks->get_heater_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = deactivate_heater_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M106 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id =
                                deactivate_heater_message.id + 1});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }

                AND_WHEN("sending an ack with error back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = deactivate_heater_message.id,
                            .with_error = errors::ErrorCode::
                                HEATER_HARDWARE_ERROR_LATCH});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "gcode response ERR211:heater:heatpad "
                                         "thermistor "
                                         "overtemp or disconnected OK\n"));
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
                                                 "M105 C:47.00 T:0.00 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 23);
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                        REQUIRE_THAT(
                            tx_buf,
                            Catch::Matchers::StartsWith(
                                "gcode response ERR206:heater:thermistor b "
                                "short OK\n"));
                    }
                }
            }
        }

        WHEN("sending a get-temp-debug") {
            auto message_text = std::string("M105.D\n");
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
                REQUIRE(std::holds_alternative<
                        messages::GetTemperatureDebugMessage>(heater_message));
                auto get_temp_message =
                    std::get<messages::GetTemperatureDebugMessage>(
                        heater_message);
                tasks->get_heater_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetTemperatureDebugResponse{
                            .responding_to_id = get_temp_message.id,
                            .pad_a_temperature = 100.0,
                            .pad_b_temperature = 42.0,
                            .board_temperature = 22,
                            .pad_a_adc = 14420,
                            .pad_b_adc = 0,
                            .board_adc = 2220});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should respond to the get-temp-debug "
                        "message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "M105.D AT:100.00 BT:42.00 OT:22.00 "
                                         "AD:14420 BD:0 OD:2220 PG:0 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetTemperatureDebugResponse{
                            .responding_to_id = get_temp_message.id + 1,
                            .pad_a_temperature = 21,
                            .pad_b_temperature = 19,
                            .board_temperature = -1,
                            .pad_a_adc = 22,
                            .pad_b_adc = 45,
                            .board_adc = 1231});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                                                 "M123 C:1500 T:1750 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 22);
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
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
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }

        WHEN("sending a get-system-info") {
            auto message_text = std::string("M115\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the system and not "
                "immediately ack") {
                REQUIRE(tasks->get_system_queue().backing_deque.size() != 0);
                auto system_message =
                    tasks->get_system_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::GetSystemInfoMessage>(
                    system_message));
                auto get_system_info_message =
                    std::get<messages::GetSystemInfoMessage>(system_message);
                tasks->get_system_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetSystemInfoResponse{
                            .responding_to_id = get_system_info_message.id,
                            .serial_number =
                                std::array<char,
                                           SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                                    "TESTSN8"},
                            .fw_version = "v1.0.1",
                            .hw_version = "v1.0.1"});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "M115 FW:v1.0.1 HW:v1.0.1 "
                                                 "SerialNo:TESTSN8 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 45);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetSystemInfoResponse{
                            .responding_to_id = get_system_info_message.id + 1,
                            .serial_number =
                                std::array<char,
                                           SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                                    "TESTSN8"},
                            .fw_version = "v1.0.1",
                            .hw_version = "v1.0.1"});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong message type back to the "
                    "comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = get_system_info_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }

        WHEN("sending a get-platelock-state-debug") {
            auto message_text = std::string("M241.D\n");
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
                REQUIRE(std::holds_alternative<
                        messages::GetPlateLockStateDebugMessage>(
                    motor_message));
                auto get_platelock_state_debug_message =
                    std::get<messages::GetPlateLockStateDebugMessage>(
                        motor_message);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetPlateLockStateDebugResponse{
                            .responding_to_id =
                                get_platelock_state_debug_message.id,
                            .plate_lock_state =
                                std::array<char, 14>{"IDLE_UNKNOWN"},
                            .plate_lock_open_state = true,
                            .plate_lock_closed_state = true});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "M241.D STATUS:IDLE_UNKNOWN "
                                         "OpenSensor:1 ClosedSensor:1 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 58);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetPlateLockStateDebugResponse{
                            .responding_to_id =
                                get_platelock_state_debug_message.id + 1,
                            .plate_lock_state =
                                std::array<char, 14>{"IDLE_UNKNOWN"},
                            .plate_lock_open_state = true,
                            .plate_lock_closed_state = true});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong message type back to the "
                    "comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id =
                                get_platelock_state_debug_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }
        WHEN("sending a GetOffsetConstants message") {
            auto message_text = std::string("M117\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass on the message and not immediately ack") {
                REQUIRE(tasks->get_heater_queue().has_message());
                auto heater_msg =
                    tasks->get_heater_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::GetOffsetConstantsMessage>(
                        heater_msg));
                auto message =
                    std::get<messages::GetOffsetConstantsMessage>(heater_msg);
                REQUIRE(!tasks->get_host_comms_queue().has_message());
                REQUIRE(written_firstpass == tx_buf.begin());
                AND_WHEN("sending good response back") {
                    auto response = messages::HostCommsMessage(
                        messages::GetOffsetConstantsResponse{
                            .responding_to_id = message.id,
                            .const_b = 10.0,
                            .const_c = 15.0});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        auto response = "M117 B:10.0000 C:15.0000 OK\n";
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(response));
                        REQUIRE(written_secondpass ==
                                tx_buf.begin() + strlen(response));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending invalid ID back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetOffsetConstantsResponse{
                            .responding_to_id = message.id + 1,
                            .const_b = 10.0,
                            .const_c = 15.0});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN(
                        "the task should pull the message and print an error") {
                        REQUIRE(written_secondpass > tx_buf.begin());
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "gcode response ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }
    }
}

SCENARIO("message handling for m301") {
    GIVEN("a host_comms task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
        WHEN("sending a motor-target set-PID-constant message") {
            std::string message_text = "M301 TM P12.0 I221.5 D-1.2\n";
            auto message_obj = messages::IncomingMessageFromHost(
                &*message_text.begin(), &*message_text.end());
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            tasks->get_host_comms_task().run_once(tx_buf.begin(), tx_buf.end());
            THEN("the gcode is parsed and sent to the motor") {
                REQUIRE(!tasks->get_motor_queue().backing_deque.empty());
                auto motor_message =
                    tasks->get_motor_queue().backing_deque.front();
                REQUIRE_THAT(
                    std::get<messages::SetPIDConstantsMessage>(motor_message)
                        .kp,
                    Catch::Matchers::WithinAbs(12.0, 0.1));
                REQUIRE_THAT(
                    std::get<messages::SetPIDConstantsMessage>(motor_message)
                        .ki,
                    Catch::Matchers::WithinAbs(221.5, 0.5));
                REQUIRE_THAT(
                    std::get<messages::SetPIDConstantsMessage>(motor_message)
                        .kd,
                    Catch::Matchers::WithinAbs(-1.2, 0.01));
            }
        }

        WHEN("sending a heater-target set-PID-constant message") {
            std::string message_text = "M301 TH P0 I-25 D12.1\n";
            auto message_obj = messages::IncomingMessageFromHost(
                &*message_text.begin(), &*message_text.end());
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            tasks->get_host_comms_task().run_once(tx_buf.begin(), tx_buf.end());
            THEN("the gcode is parsed and sent to the heater") {
                REQUIRE(!tasks->get_heater_queue().backing_deque.empty());
                auto heater_message =
                    tasks->get_heater_queue().backing_deque.front();
                REQUIRE_THAT(
                    std::get<messages::SetPIDConstantsMessage>(heater_message)
                        .kp,
                    Catch::Matchers::WithinAbs(0, 0.01));
                REQUIRE_THAT(
                    std::get<messages::SetPIDConstantsMessage>(heater_message)
                        .ki,
                    Catch::Matchers::WithinAbs(-25, 0.1));
                REQUIRE_THAT(
                    std::get<messages::SetPIDConstantsMessage>(heater_message)
                        .kd,
                    Catch::Matchers::WithinAbs(12.1, 0.1));
            }
        }
    }
}

SCENARIO("message handling for other-task-initiated communication") {
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
                REQUIRE_THAT(tx_buf,
                             Catch::Matchers::StartsWith(
                                 "ERR120:main motor:illegal speed OK\n"));
                REQUIRE(*written == 'c');
            }
        }
        WHEN("sending a force-disconnect") {
            auto message_obj = messages::ForceUSBDisconnectMessage{.id = 222};
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written = tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                                 tx_buf.end());
            static_cast<void>(written);
            THEN("the task should acknowledge") {
                REQUIRE(!tasks->get_system_queue().backing_deque.empty());
                REQUIRE(std::get<messages::AcknowledgePrevious>(
                            tasks->get_system_queue().backing_deque.front())
                            .responding_to_id == message_obj.id);
            }
            THEN("the task should disconnect") {
                REQUIRE(!tasks->get_host_comms_task().may_connect());
            }
        }
    }
}
