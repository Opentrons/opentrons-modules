#include <string>

#include "catch2/catch.hpp"
#include "heater-shaker/messages.hpp"
#include "test/task_builder.hpp"

SCENARIO("testing full message passing integration") {
    GIVEN("a full set of tasks") {
        auto tasks = TaskBuilder::build();
        WHEN("sending a set-rpm message by string to the host comms task") {
            std::string message_str = "M3 S2000\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_str.begin(), &*message_str.end())));
            THEN("after spinning both tasks, we get a response") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE(written == response_buffer.begin());
                tasks->get_motor_task().run_once(tasks->get_motor_policy());
                written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE_THAT(response_buffer,
                             Catch::Matchers::StartsWith("M3 OK\n"));
            }
        }
        WHEN("sending a get-rpm message by string to the host comms task") {
            std::string message_str = "M123\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_str.begin(), &*message_str.end())));
            THEN("after spinning both tasks, we get a response") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE(written == response_buffer.begin());
                tasks->get_motor_policy().test_set_current_rpm(1050);
                tasks->get_motor_policy().set_rpm(3500);
                tasks->get_motor_task().run_once(tasks->get_motor_policy());
                written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE_THAT(response_buffer, Catch::Matchers::StartsWith(
                                                  "M123 C1050 T3500 OK\n"));
            }
        }
        WHEN("sending a set-temp message by string to the host comms task") {
            std::string message_str = "M104 S75\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_str.begin(), &*message_str.end())));
            THEN("after spinning both tasks, we get a response") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE(written == response_buffer.begin());
                tasks->get_heater_task().run_once();
                written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE_THAT(response_buffer,
                             Catch::Matchers::StartsWith("M104 OK\n"));
            }
        }

        WHEN("sending a get-temp message by string to the host comms task") {
            std::string message_str = "M105\n";
            tasks->get_host_comms_queue().backing_deque.push_back(
                messages::HostCommsMessage(messages::IncomingMessageFromHost(
                    &*message_str.begin(), &*message_str.end())));
            THEN("after spinning both tasks, we get a reponse") {
                auto response_buffer = std::string(64, 'c');
                auto written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE(written == response_buffer.begin());
                tasks->get_heater_task().run_once();
                written = tasks->get_host_comms_task().run_once(
                    response_buffer.begin(), response_buffer.end());
                REQUIRE_THAT(response_buffer,
                             Catch::Matchers::StartsWith("M105 C99 T48 OK\n"));
            }
        }
    }
}
