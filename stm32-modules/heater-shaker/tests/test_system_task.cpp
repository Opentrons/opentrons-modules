#include "catch2/catch.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/system_task.hpp"
#include "test/task_builder.hpp"
#include "systemwide.hpp"

SCENARIO("system task message passing") {
    GIVEN("a system task") {
        auto tasks = TaskBuilder::build();

        WHEN("sending an enter-bootloader message") {
            auto message = messages::EnterBootloaderMessage{.id = 222};
            tasks->get_system_queue().backing_deque.push_back(message);
            tasks->run_system_task();
            THEN("the system task should not enter the bootloader") {
                REQUIRE(!tasks->get_system_policy().bootloader_entered());
            }
            THEN("the system task should pass on a disable-heater message") {
                REQUIRE(!tasks->get_heater_queue().backing_deque.empty());
                auto message = tasks->get_heater_queue().backing_deque.front();
                tasks->get_heater_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::SetTemperatureMessage>(message)
                            .target_temperature == 0);
                REQUIRE(std::get<messages::SetTemperatureMessage>(message)
                            .from_system);
            }
            THEN("the system task should pass on a disable-motor message") {
                REQUIRE(!tasks->get_motor_queue().backing_deque.empty());
                auto message = tasks->get_motor_queue().backing_deque.front();
                tasks->get_motor_queue().backing_deque.pop_front();
                REQUIRE(std::get<messages::SetRPMMessage>(message).target_rpm ==
                        0);
                REQUIRE(std::get<messages::SetRPMMessage>(message).from_system);
            }
            THEN("the system task should pass on a disconnect-usb message") {
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto dc_message =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(
                    std::holds_alternative<messages::ForceUSBDisconnectMessage>(
                        dc_message));
                AND_THEN("the system task should acknowledge to usb") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto ack =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(std::get<messages::AcknowledgePrevious>(ack)
                                .responding_to_id == message.id);
                    REQUIRE(
                        tasks->get_host_comms_queue().backing_deque.empty());
                }
            }
            AND_WHEN("parsing acknowledgements") {
                auto heater_ack = messages::AcknowledgePrevious{
                    .responding_to_id =
                        std::get<messages::SetTemperatureMessage>(
                            tasks->get_heater_queue().backing_deque.front())
                            .id};

                auto motor_ack = messages::AcknowledgePrevious{
                    .responding_to_id =
                        std::get<messages::SetRPMMessage>(
                            tasks->get_motor_queue().backing_deque.front())
                            .id};
                auto usb_ack = messages::AcknowledgePrevious{
                    .responding_to_id =
                        std::get<messages::ForceUSBDisconnectMessage>(
                            tasks->get_host_comms_queue().backing_deque.front())
                            .id};
                tasks->get_system_queue().backing_deque.push_front(usb_ack);
                tasks->get_system_queue().backing_deque.push_front(motor_ack);
                tasks->get_system_queue().backing_deque.push_front(heater_ack);
                THEN(
                    "the system should not enter the bootloader until all acks "
                    "are in") {
                    tasks->run_system_task();
                    REQUIRE(!tasks->get_system_policy().bootloader_entered());

                    tasks->run_system_task();
                    REQUIRE(!tasks->get_system_policy().bootloader_entered());

                    tasks->run_system_task();
                    REQUIRE(tasks->get_system_policy().bootloader_entered());
                }
            }
        }

        WHEN("sending a set-serial-number message as if from the host comms") {
            auto message = messages::SetSerialNumberMessage{
                .id = 123, .serial_number = std::array<char, systemwide::serial_number_length>{"TESTSN4"}};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_system_queue().backing_deque.empty());
                AND_THEN("the task should set the serial number") {
                    REQUIRE(tasks->get_system_policy().get_serial_number() ==
                            std::array<char, systemwide::serial_number_length>{"TESTSN4"});
                }
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    REQUIRE(tasks->get_system_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto ack =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(ack.responding_to_id == message.id);
                }
            }
        }
    }
}
