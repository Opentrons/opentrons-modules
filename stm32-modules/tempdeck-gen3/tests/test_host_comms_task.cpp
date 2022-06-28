#include "catch2/catch.hpp"
#include "test/test_tasks.hpp"

SCENARIO("usb message parsing") {
    GIVEN("a host_comms_task") {
        auto *tasks = new tasks::TestTasks();
        std::string tx_buf(128, 'c');
        WHEN("calling run_once() with nothing in the queue") {
            THEN("the task should call recv(), which should throw") {
                REQUIRE_THROWS(
                    tasks->_comms_task.run_once(tx_buf.begin(), tx_buf.end()));
            }
        }
        WHEN("calling run_once() with an empty gcode message") {
            auto message_text = std::string("\n");
            auto message_obj =
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_text.begin(), &*message_text.end()));
            tasks->_comms_queue.backing_deque.push_back(message_obj);
            THEN("the task should call recv() and get the message") {
                REQUIRE_NOTHROW(
                    tasks->_comms_task.run_once(tx_buf.begin(), tx_buf.end()));
                REQUIRE(tasks->_comms_queue.backing_deque.empty());
            }
            THEN("the task writes nothing to the transmit buffer") {
                auto written =
                    tasks->_comms_task.run_once(tx_buf.begin(), tx_buf.end());
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
            tasks->_comms_queue.backing_deque.push_back(message_obj);
            std::string small_buf(64, 'c');
            auto ends_at =
                errors::write_into(small_buf.begin(), small_buf.end(),
                                   errors::ErrorCode::USB_TX_OVERRUN);
            small_buf.resize(ends_at - small_buf.begin() - 5);
            auto written =
                tasks->_comms_task.run_once(small_buf.begin(), small_buf.end());
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
            tasks->_comms_queue.backing_deque.push_back(message_obj);
            THEN("the task writes an error to the transmit buffer") {
                auto written =
                    tasks->_comms_task.run_once(tx_buf.begin(), tx_buf.end());
                REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(
                                         "ERR003:unhandled gcode\n"));
                REQUIRE(written ==
                        tx_buf.begin() + strlen("ERR003:unhandled gcode\n"));
            }
        }
    }
}

SCENARIO("host comms commands to system task") {
    auto *tasks = tasks::BuildTasks();
    std::string tx_buf(128, 'c');
    WHEN("sending gcode M115") {
        auto message_text = std::string("M115\n");
        auto message_obj =
            messages::HostCommsMessage(messages::IncomingMessageFromHost(
                &*message_text.begin(), &*message_text.end()));
        REQUIRE(tasks->_comms_queue.try_send(message_obj));
        auto written =
            tasks->_comms_task.run_once(tx_buf.begin(), tx_buf.end());
        THEN("the task does not immediately ack") {
            REQUIRE(written == tx_buf.begin());
        }
        THEN("a message is sent to the system task") {
            REQUIRE(tasks->_system_queue.has_message());
            auto sys_msg = tasks->_system_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::GetSystemInfoMessage>(
                sys_msg));
            auto id = std::get<messages::GetSystemInfoMessage>(sys_msg).id;
            AND_WHEN("sending a response") {
                auto resp = messages::GetSystemInfoResponse{
                    .responding_to_id = id,
                    .serial_number = {'a', 'b', 'c'},
                    .fw_version = "def",
                    .hw_version = "ghi"};
                REQUIRE(tasks->_comms_queue.try_send(resp));
                written =
                    tasks->_comms_task.run_once(tx_buf.begin(), tx_buf.end());
                THEN("an ack is sent") {
                    auto expected = "M115 FW:def HW:ghi SerialNo:abc OK\n";
                    REQUIRE(written != tx_buf.begin());
                    REQUIRE_THAT(tx_buf, Catch::Matchers::StartsWith(expected));
                }
            }
        }
    }
}
