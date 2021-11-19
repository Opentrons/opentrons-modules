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
    }
}

SCENARIO("message passing for response-carrying gcodes from usb input") {
    GIVEN("a host_comms task") {
        auto tasks = TaskBuilder::build();
        std::string tx_buf(128, 'c');
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
