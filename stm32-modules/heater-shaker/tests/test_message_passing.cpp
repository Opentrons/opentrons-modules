#include <string>
#include "catch2/catch.hpp"
#include "test/task_builder.hpp"
#include "heater-shaker/messages.hpp"

SCENARIO("testing full message passing integration") {
    GIVEN("a full set of tasks") {
        auto tasks = TaskBuilder::build();
        WHEN("sending a set-rpm message by string to the host comms task") {
            std::string message_str = "M3 S2000\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(
                    messages::IncomingMessageFromHost(message_str.data(), message_str.size())));
            THEN("after spinning both tasks, we get a response") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE(written == 0);
                tasks->get_motor_task().run_once();
                written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE_THAT(response_buffer, Catch::Matchers::StartsWith("M3 OK\n"));
            }
        }
        WHEN("sending a get-rpm message by string to the host comms task") {
            std::string message_str = "M123\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(
                    messages::IncomingMessageFromHost(message_str.data(), message_str.size())));
            THEN("after spinning both tasks, we get a response") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE(written == 0);
                tasks->get_motor_task().run_once();
                written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE_THAT(response_buffer, Catch::Matchers::StartsWith("M123 C1050 T3500 OK\n"));
            }
        }
        WHEN("sending a set-temp message by string to the host comms task") {
            std::string message_str = "M104 S75\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(
                    messages::IncomingMessageFromHost(message_str.data(), message_str.size())));
            THEN("after spinning both tasks, we get a response") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE(written == 0);
                tasks->get_heater_task().run_once();
                written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE_THAT(response_buffer, Catch::Matchers::StartsWith("M104 OK\n"));
            }
        }

        WHEN("sending a get-temp message by string to the host comms task") {
            std::string message_str = "M105\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(
                    messages::IncomingMessageFromHost(message_str.data(), message_str.size())));
            THEN("after spinning both tasks, we get a reponse") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE(written == 0);
                tasks->get_heater_task().run_once();
                written = tasks->get_host_comms_task().run_once(response_buffer.data(), response_buffer.size());
                REQUIRE_THAT(response_buffer, Catch::Matchers::StartsWith("M105 C99 T48 OK\n"));
            }
        }
    }
}
