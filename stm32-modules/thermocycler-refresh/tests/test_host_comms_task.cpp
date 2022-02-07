#include <cstring>
#include <string>

#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/messages.hpp"

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
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
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
                                "ERR302:system:HAL error, busy, or timeout\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetFanManual message") {
            std::string message_text = std::string("M106 S0.5\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the thermal plate task "
                "and not immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto plate_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                auto set_fan_manual_message =
                    std::get<messages::SetFanManualMessage>(plate_message);
                tasks->get_system_queue().backing_deque.pop_front();
                constexpr double test_power = 0.5F;
                REQUIRE(set_fan_manual_message.power == test_power);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_fan_manual_message.id});
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
                            .responding_to_id = set_fan_manual_message.id + 1});
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
                            .responding_to_id = set_fan_manual_message.id,
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
                                "ERR302:system:HAL error, busy, or timeout\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetFanAutomatic message") {
            std::string message_text = std::string("M107\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the thermal plate task "
                "and not immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto plate_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                auto set_fan_auto_message =
                    std::get<messages::SetFanAutomaticMessage>(plate_message);
                tasks->get_system_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_fan_auto_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M107 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_fan_auto_message.id + 1});
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
                            .responding_to_id = set_fan_auto_message.id,
                            .with_error =
                                errors::ErrorCode::THERMAL_HEATSINK_FAN_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "ERR403:thermal:Could not "
                                                 "control heatsink fan\n"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetLidTemperature message") {
            std::string message_text = std::string("M140 S101.0\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the lid heater task "
                "and not immediately ack") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.size() !=
                        0);
                auto lid_message =
                    tasks->get_lid_heater_queue().backing_deque.front();
                auto set_lid_temp_message =
                    std::get<messages::SetLidTemperatureMessage>(lid_message);
                tasks->get_lid_heater_queue().backing_deque.pop_front();
                constexpr double test_temp = 101.0F;
                REQUIRE(set_lid_temp_message.setpoint == test_temp);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_lid_temp_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M140 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_lid_temp_message.id + 1});
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
                            .responding_to_id = set_lid_temp_message.id,
                            .with_error =
                                errors::ErrorCode::THERMAL_HEATER_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR405:"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a DeactivateLidHeating message") {
            std::string message_text = std::string("M108\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the lid heater task "
                "and not immediately ack") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.size() !=
                        0);
                auto lid_message =
                    tasks->get_lid_heater_queue().backing_deque.front();
                auto deactivate_lid_message =
                    std::get<messages::DeactivateLidHeatingMessage>(
                        lid_message);
                tasks->get_lid_heater_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = deactivate_lid_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M108 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = deactivate_lid_message.id + 1});
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
                            .responding_to_id = deactivate_lid_message.id,
                            .with_error =
                                errors::ErrorCode::THERMAL_HEATER_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR405:"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetPlateTemperature message") {
            std::string message_text = std::string("M104 S95.0 H40\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the lid heater task "
                "and not immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto lid_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                auto set_plate_temp_message =
                    std::get<messages::SetPlateTemperatureMessage>(lid_message);
                tasks->get_thermal_plate_queue().backing_deque.pop_front();
                constexpr double test_temp = 95.0F;
                constexpr double test_hold = 40.0F;
                REQUIRE(set_plate_temp_message.setpoint == test_temp);
                REQUIRE(set_plate_temp_message.hold_time == test_hold);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_plate_temp_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M104 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_plate_temp_message.id + 1});
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
                            .responding_to_id = set_plate_temp_message.id,
                            .with_error =
                                errors::ErrorCode::THERMAL_HEATER_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR405:"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a DeactivatePlate message") {
            std::string message_text = std::string("M14\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the lid heater task "
                "and not immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto plate_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                auto deactivate_plate_message =
                    std::get<messages::DeactivatePlateMessage>(plate_message);
                tasks->get_thermal_plate_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = deactivate_plate_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M14 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id =
                                deactivate_plate_message.id + 1});
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
                            .responding_to_id = deactivate_plate_message.id,
                            .with_error =
                                errors::ErrorCode::THERMAL_PELTIER_ERROR});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR402:"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetPIDConstants message for the heaters") {
            std::string message_text = std::string("M301 SH P1 I1 D1\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the lid heater task "
                "and not immediately ack") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.size() !=
                        0);
                auto lid_message =
                    tasks->get_lid_heater_queue().backing_deque.front();
                auto set_lid_temp_message =
                    std::get<messages::SetPIDConstantsMessage>(lid_message);
                tasks->get_lid_heater_queue().backing_deque.pop_front();
                REQUIRE(set_lid_temp_message.selection == PidSelection::HEATER);
                REQUIRE(set_lid_temp_message.p == 1.0);
                REQUIRE(set_lid_temp_message.i == 1.0);
                REQUIRE(set_lid_temp_message.d == 1.0);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_lid_temp_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M301 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_lid_temp_message.id + 1});
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
                            .responding_to_id = set_lid_temp_message.id,
                            .with_error = errors::ErrorCode::THERMAL_LID_BUSY});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR404:"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetPIDConstants message for the peltiers") {
            std::string message_text = std::string("M301 SP P1 I1 D1\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the thermal plate task "
                "and not immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto plate_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                auto set_plate_pid_message =
                    std::get<messages::SetPIDConstantsMessage>(plate_message);
                tasks->get_thermal_plate_queue().backing_deque.pop_front();
                REQUIRE(set_plate_pid_message.selection ==
                        PidSelection::PELTIERS);
                REQUIRE(set_plate_pid_message.p == 1.0);
                REQUIRE(set_plate_pid_message.i == 1.0);
                REQUIRE(set_plate_pid_message.d == 1.0);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_plate_pid_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M301 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_plate_pid_message.id + 1});
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
                            .responding_to_id = set_plate_pid_message.id,
                            .with_error =
                                errors::ErrorCode::THERMAL_PLATE_BUSY});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR401:"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                        REQUIRE(written_secondpass != tx_buf.begin());
                    }
                }
            }
        }
        WHEN("sending a SetPIDConstants message for the fans") {
            std::string message_text = std::string("M301 SF P1 I1 D1\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the thermal plate task "
                "and not immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto plate_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                auto set_plate_pid_message =
                    std::get<messages::SetPIDConstantsMessage>(plate_message);
                tasks->get_thermal_plate_queue().backing_deque.pop_front();
                REQUIRE(set_plate_pid_message.selection == PidSelection::FANS);
                REQUIRE(set_plate_pid_message.p == 1.0);
                REQUIRE(set_plate_pid_message.i == 1.0);
                REQUIRE(set_plate_pid_message.d == 1.0);
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_plate_pid_message.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("M301 OK\n"));
                        REQUIRE(written_secondpass != tx_buf.begin());
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending a bad response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = set_plate_pid_message.id + 1});
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
                            .responding_to_id = set_plate_pid_message.id,
                            .with_error =
                                errors::ErrorCode::THERMAL_PLATE_BUSY});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should print the error rather than ack") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR401:"));
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
        std::string tx_buf(256, 'c');
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
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("ERR005"));
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
            }
        }
        WHEN("sending a get-lid-temp message") {
            auto message_text = std::string("M141\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the lid and not "
                "immediately ack") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.size() !=
                        0);
                auto lid_message =
                    tasks->get_lid_heater_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::GetLidTempMessage>(
                    lid_message));
                auto get_lid_temp_message =
                    std::get<messages::GetLidTempMessage>(lid_message);
                tasks->get_lid_heater_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response =
                        messages::HostCommsMessage(messages::GetLidTempResponse{
                            .responding_to_id = get_lid_temp_message.id,
                            .current_temp = 30.0F,
                            .set_temp = 35.0F});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "M141 T:35.00 C:30.00 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 24);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response =
                        messages::HostCommsMessage(messages::GetLidTempResponse{
                            .responding_to_id = get_lid_temp_message.id + 1,
                            .current_temp = 30.0F,
                            .set_temp = 35.0F});
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
                        messages::AcknowledgePrevious{
                            .responding_to_id = get_lid_temp_message.id});
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
        WHEN("sending a get-plate-temp message") {
            auto message_text = std::string("M105\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the plate task and not "
                "immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto plate_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::GetPlateTempMessage>(
                    plate_message));
                auto get_plate_temp_message =
                    std::get<messages::GetPlateTempMessage>(plate_message);
                tasks->get_thermal_plate_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetPlateTempResponse{
                            .responding_to_id = get_plate_temp_message.id,
                            .current_temp = 30.0F,
                            .set_temp = 35.0F});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                                 "M105 T:35.00 C:30.00 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 24);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetPlateTempResponse{
                            .responding_to_id = get_plate_temp_message.id + 1,
                            .current_temp = 30.0F,
                            .set_temp = 35.0F});
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
                        messages::AcknowledgePrevious{
                            .responding_to_id = get_plate_temp_message.id});
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
        WHEN("sending a get-lid-debug-temp message") {
            auto message_text = std::string("M141.D\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the lid and not "
                "immediately ack") {
                REQUIRE(tasks->get_lid_heater_queue().backing_deque.size() !=
                        0);
                auto lid_message =
                    tasks->get_lid_heater_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<
                        messages::GetLidTemperatureDebugMessage>(lid_message));
                auto get_lid_temp_message =
                    std::get<messages::GetLidTemperatureDebugMessage>(
                        lid_message);
                tasks->get_lid_heater_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetLidTemperatureDebugResponse{
                            .responding_to_id = get_lid_temp_message.id,
                            .lid_temp = 30.0F,
                            .lid_adc = 123});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith(
                                         "M141.D LT:30.00 LA:123 OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 26);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetLidTemperatureDebugResponse{
                            .responding_to_id = get_lid_temp_message.id + 1,
                            .lid_temp = 30.0F,
                            .lid_adc = 123});
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
                        messages::AcknowledgePrevious{
                            .responding_to_id = get_lid_temp_message.id});
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
        WHEN("sending a get-plate-temp-debug message") {
            auto message_text = std::string("M105.D\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN(
                "the task should pass the message on to the plate task and not "
                "immediately ack") {
                REQUIRE(tasks->get_thermal_plate_queue().backing_deque.size() !=
                        0);
                auto plate_message =
                    tasks->get_thermal_plate_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<
                        messages::GetPlateTemperatureDebugMessage>(
                    plate_message));
                auto get_plate_temp_message =
                    std::get<messages::GetPlateTemperatureDebugMessage>(
                        plate_message);
                tasks->get_thermal_plate_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
                AND_WHEN("sending a good response back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetPlateTemperatureDebugResponse{
                            .responding_to_id = get_plate_temp_message.id,
                            .heat_sink_temp = 30,
                            .front_right_temp = 30,
                            .front_center_temp = 30,
                            .front_left_temp = 30,
                            .back_right_temp = 30,
                            .back_center_temp = 30,
                            .back_left_temp = 30,
                            .heat_sink_adc = 123,
                            .front_right_adc = 123,
                            .front_center_adc = 123,
                            .front_left_adc = 123,
                            .back_right_adc = 123,
                            .back_center_adc = 123,
                            .back_left_adc = 123});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(
                            tx_buf,
                            Catch::Matchers::StartsWith(
                                "M105.D HST:30.00 FRT:30.00 FLT:30.00 "
                                "FCT:30.00 "
                                "BRT:30.00 BLT:30.00 BCT:30.00 HSA:123 FRA:123 "
                                "FLA:123 FCA:123 BRA:123 BLA:123 BCA:123 "
                                "OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 136);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN(
                    "sending a response with wrong id back to the comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetPlateTemperatureDebugResponse{
                            .responding_to_id = get_plate_temp_message.id + 1,
                            .heat_sink_temp = 30,
                            .front_right_temp = 30,
                            .front_center_temp = 30,
                            .front_left_temp = 30,
                            .back_right_temp = 30,
                            .back_center_temp = 30,
                            .back_left_temp = 30,
                            .heat_sink_adc = 123,
                            .front_right_adc = 123,
                            .front_center_adc = 123,
                            .front_left_adc = 123,
                            .back_right_adc = 123,
                            .back_center_adc = 123,
                            .back_left_adc = 123});
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
                        messages::AcknowledgePrevious{
                            .responding_to_id = get_plate_temp_message.id});
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
        WHEN("sending an ActuateSolenoid command") {
            auto message_text = std::string("G28.D 1\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN("the task should pass the message and not immediately ack") {
                REQUIRE(tasks->get_motor_queue().has_message());
                auto motor_msg = tasks->get_motor_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::ActuateSolenoidMessage>(
                        motor_msg));
                auto actuate_solenoid_msg =
                    std::get<messages::ActuateSolenoidMessage>(motor_msg);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(!tasks->get_host_comms_queue().has_message());
                REQUIRE(actuate_solenoid_msg.engage);
                AND_WHEN("sending good response back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = actuate_solenoid_msg.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(tx_buf,
                                     Catch::Matchers::StartsWith("G28.D OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 9);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending invalid ID back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = actuate_solenoid_msg.id + 1});
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
        WHEN("sending a LidStepperDebugMessage command") {
            auto message_text = std::string("M240.D 10\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN("the task should pass the message and not immediately ack") {
                REQUIRE(tasks->get_motor_queue().has_message());
                auto motor_msg = tasks->get_motor_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::LidStepperDebugMessage>(
                        motor_msg));
                auto lid_stepper_msg =
                    std::get<messages::LidStepperDebugMessage>(motor_msg);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(!tasks->get_host_comms_queue().has_message());
                REQUIRE(lid_stepper_msg.angle == 10.0F);
                AND_WHEN("sending good response back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          lid_stepper_msg.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(
                            tx_buf, Catch::Matchers::StartsWith("M240.D OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 10);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending invalid ID back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = lid_stepper_msg.id + 1});
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
        WHEN("sending a SealStepperDebugMessage command") {
            auto message_text = std::string("M241.D 10\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN("the task should pass the message and not immediately ack") {
                REQUIRE(tasks->get_motor_queue().has_message());
                auto motor_msg = tasks->get_motor_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::SealStepperDebugMessage>(
                        motor_msg));
                auto seal_stepper_msg =
                    std::get<messages::SealStepperDebugMessage>(motor_msg);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(!tasks->get_host_comms_queue().has_message());
                REQUIRE(seal_stepper_msg.steps == 10);
                AND_WHEN("sending good response back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          seal_stepper_msg.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        REQUIRE_THAT(
                            tx_buf, Catch::Matchers::StartsWith("M241.D OK\n"));
                        REQUIRE(written_secondpass == tx_buf.begin() + 10);
                        REQUIRE(tasks->get_host_comms_queue()
                                    .backing_deque.empty());
                    }
                }
                AND_WHEN("sending invalid ID back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{
                            .responding_to_id = seal_stepper_msg.id + 1});
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
        WHEN("sending a GetSealDriveStatus command") {
            auto message_text = std::string("M242.D\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN("the task should pass the message and not immediately ack") {
                REQUIRE(tasks->get_motor_queue().has_message());
                auto motor_msg = tasks->get_motor_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::GetSealDriveStatusMessage>(
                        motor_msg));
                auto seal_stepper_msg =
                    std::get<messages::GetSealDriveStatusMessage>(motor_msg);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(!tasks->get_host_comms_queue().has_message());
                AND_WHEN("sending good response back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::GetSealDriveStatusResponse{
                            .responding_to_id = seal_stepper_msg.id,
                            .status = tmc2130::DriveStatus()});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        const char response[] = "M242.D SG:0 SG_Result:0 OK\n";
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
                        messages::GetSealDriveStatusResponse{
                            .responding_to_id = seal_stepper_msg.id + 1,
                            .status = tmc2130::DriveStatus()});
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
        WHEN("sending a SetSealParameter command") {
            auto message_text = std::string("M243.D V 10000\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->get_host_comms_queue().backing_deque.push_back(message_obj);
            auto written_firstpass = tasks->get_host_comms_task().run_once(
                tx_buf.begin(), tx_buf.end());
            THEN("the task should pass the message and not immediately ack") {
                REQUIRE(tasks->get_motor_queue().has_message());
                auto motor_msg = tasks->get_motor_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::SetSealParameterMessage>(
                        motor_msg));
                auto seal_stepper_msg =
                    std::get<messages::SetSealParameterMessage>(motor_msg);
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(written_firstpass == tx_buf.begin());
                REQUIRE(!tasks->get_host_comms_queue().has_message());
                AND_WHEN("sending good response back to comms task") {
                    auto response = messages::HostCommsMessage(
                        messages::AcknowledgePrevious{.responding_to_id =
                                                          seal_stepper_msg.id});
                    tasks->get_host_comms_queue().backing_deque.push_back(
                        response);
                    auto written_secondpass =
                        tasks->get_host_comms_task().run_once(tx_buf.begin(),
                                                              tx_buf.end());
                    THEN("the task should ack the previous message") {
                        const char response[] = "M243.D OK\n";
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
                        messages::AcknowledgePrevious{
                            .responding_to_id = seal_stepper_msg.id + 1});
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

SCENARIO("message handling for other-task-initiated communication") {
    GIVEN("a host_comms task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
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
