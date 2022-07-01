#include "catch2/catch.hpp"
#include "test/test_system_policy.hpp"
#include "test/test_tasks.hpp"

SCENARIO("system task bootloader") {
    auto *tasks = tasks::BuildTasks();
    TestSystemPolicy policy;
    WHEN("sending an Enter Bootloader message to system task") {
        auto msg = messages::EnterBootloaderMessage{.id = 123};
        tasks->_system_queue.backing_deque.push_back(msg);
        tasks->_system_task.run_once(policy);
        THEN("the system task doesn't enter the bootloader") {
            REQUIRE(policy._bootloader_count == 0);
        }
        THEN("system task sends a Force USB Disconnect") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(
                std::holds_alternative<messages::ForceUSBDisconnect>(host_msg));
            auto system_msg = std::get<messages::ForceUSBDisconnect>(host_msg);
            REQUIRE(system_msg.return_address ==
                    tasks::TestTasks::Queues::SystemAddress);
            auto reply_id = system_msg.id;
            AND_WHEN("host comms acknowledges the USB disconnect") {
                auto ack =
                    messages::AcknowledgePrevious{.responding_to_id = reply_id};
                tasks->_system_queue.backing_deque.push_back(ack);
                tasks->_system_task.run_once(policy);
                THEN("system task enters the bootloader") {
                    REQUIRE(policy._bootloader_count == 1);
                }
            }
        }
    }
}

SCENARIO("system task system info command") {
    auto *tasks = tasks::BuildTasks();
    TestSystemPolicy policy;
    WHEN("getting system info") {
        auto msg = messages::GetSystemInfoMessage{.id = 123};
        tasks->_system_queue.backing_deque.push_back(msg);
        tasks->_system_task.run_once(policy);
        THEN("the system task replies with the system info") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::GetSystemInfoResponse>(
                host_msg));
            auto response = std::get<messages::GetSystemInfoResponse>(host_msg);
            REQUIRE(response.responding_to_id == msg.id);
            REQUIRE(std::strncmp(response.fw_version, version::fw_version(),
                                 strlen(response.fw_version)) == 0);
            REQUIRE(std::strncmp(response.hw_version, version::hw_version(),
                                 strlen(response.hw_version)) == 0);
            const char *ser = &response.serial_number[0];
            REQUIRE_THAT(std::string(ser),
                         Catch::Matchers::StartsWith("EMPTYSN"));
        }
    }
    WHEN("setting system info") {
        auto msg = messages::SetSerialNumberMessage{.id = 123,
                                                    .serial_number = {"ABCD"}};
        tasks->_system_queue.backing_deque.push_back(msg);
        tasks->_system_task.run_once(policy);
        THEN("the system info is set correctly") {
            const char *ser = &policy._serial[0];
            REQUIRE_THAT(std::string(ser), Catch::Matchers::StartsWith("ABCD"));
            REQUIRE(policy._serial_set);
        }
        THEN("system task acks the command") {
            REQUIRE(tasks->_comms_queue.has_message());
            auto host_msg = tasks->_comms_queue.backing_deque.front();
            REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                host_msg));
            auto ack = std::get<messages::AcknowledgePrevious>(host_msg);
            REQUIRE(ack.responding_to_id == msg.id);
        }
    }
}
