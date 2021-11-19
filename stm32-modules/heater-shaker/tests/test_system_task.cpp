#include "catch2/catch.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/system_task.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"

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
                .id = 123,
                .serial_number =
                    std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                        "TESTSN4"}};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_system_queue().backing_deque.empty());
                AND_THEN("the task should set the serial number") {
                    REQUIRE(tasks->get_system_policy().get_serial_number() ==
                            std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                                "TESTSN4"});
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

        WHEN("sending a get-system-info message as if from the host comms") {
            auto message = messages::GetSystemInfoMessage{.id = 123};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_policy().set_serial_number(
                std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{"TESTSN6"});
            tasks->get_system_task().run_once(tasks->get_system_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_system_queue().backing_deque.empty());
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::GetSystemInfoResponse>(
                            response));
                    auto getsysteminfo =
                        std::get<messages::GetSystemInfoResponse>(response);
                    REQUIRE(getsysteminfo.responding_to_id == message.id);
                    REQUIRE(getsysteminfo.serial_number ==
                            std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
                                "TESTSN6"});
                }
            }
        }

        WHEN("receiving a set-LED message as if from the host task") {
            auto message = messages::SetLEDMessage{.id = 123};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            THEN("the task should process the message") {
                REQUIRE(tasks->get_system_queue().backing_deque.empty());
                AND_THEN(
                    "the task should respond to the message to the host "
                    "comms") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto response =
                        tasks->get_host_comms_queue().backing_deque.front();
                    tasks->get_host_comms_queue().backing_deque.pop_front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            response));
                    auto getackresponse =
                        std::get<messages::AcknowledgePrevious>(response);
                    REQUIRE(getackresponse.responding_to_id == message.id);
                    REQUIRE(getackresponse.with_error ==
                            errors::ErrorCode::NO_ERROR);
                }
            }
        }
        WHEN(
            "receiving an identify-module-start-set-LED message as if from the "
            "host task") {
            auto message = messages::IdentifyModuleStartLEDMessage{.id = 123};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            THEN(
                "the task should process the message and send host comms and "
                "check status messages") {
                REQUIRE(tasks->get_system_task().get_led_blink_state() ==
                        system_task::LEDBlinkState::BLINK_ON_WAITING);
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    response));
                auto getackresponse =
                    std::get<messages::AcknowledgePrevious>(response);
                REQUIRE(getackresponse.responding_to_id == message.id);
                REQUIRE(getackresponse.with_error ==
                        errors::ErrorCode::NO_ERROR);
                REQUIRE(!tasks->get_system_queue().backing_deque.empty());
                auto response2 =
                    tasks->get_system_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<
                        messages::CheckLEDBlinkStatusMessage>(response2));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                AND_THEN("the task should process the check status message") {
                    REQUIRE(tasks->get_system_task().get_led_blink_state() ==
                            system_task::LEDBlinkState::BLINK_OFF_WAITING);
                    REQUIRE(!tasks->get_system_queue().backing_deque.empty());
                    auto response3 =
                        tasks->get_system_queue().backing_deque.front();
                    tasks->get_system_queue()
                        .backing_deque
                        .pop_front();  // clear check status message
                    REQUIRE(std::holds_alternative<
                            messages::CheckLEDBlinkStatusMessage>(response3));
                    auto message2 =
                        messages::IdentifyModuleStopLEDMessage{.id = 234};
                    tasks->get_system_queue().backing_deque.push_back(
                        messages::SystemMessage(message2));
                    auto message3 = messages::CheckLEDBlinkStatusMessage{};
                    tasks->get_system_queue().backing_deque.push_back(
                        messages::SystemMessage(
                            message3));  // reintroduce check status message
                    tasks->get_system_task().run_once(
                        tasks->get_system_policy());  // process stop message
                    tasks->get_system_task().run_once(
                        tasks->get_system_policy());  // process check status
                                                      // message
                    AND_THEN(
                        "the task should process a stop blinking message and "
                        "send host comms message") {
                        REQUIRE(
                            tasks->get_system_task().get_led_blink_state() ==
                            system_task::LEDBlinkState::BLINK_OFF);
                        REQUIRE(
                            tasks->get_system_queue().backing_deque.empty());
                        REQUIRE(!tasks->get_host_comms_queue()
                                     .backing_deque.empty());
                        auto response4 =
                            tasks->get_host_comms_queue().backing_deque.front();
                        tasks->get_host_comms_queue().backing_deque.pop_front();
                        REQUIRE(std::holds_alternative<
                                messages::AcknowledgePrevious>(response4));
                        auto getackresponse2 =
                            std::get<messages::AcknowledgePrevious>(response4);
                        REQUIRE(getackresponse2.responding_to_id ==
                                message2.id);
                        REQUIRE(getackresponse2.with_error ==
                                errors::ErrorCode::NO_ERROR);
                    }
                }
            }
        }
        WHEN(
            "receiving an identify-module-stop-set-LED message as if from the "
            "host task") {
            auto message = messages::IdentifyModuleStopLEDMessage{.id = 123};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            THEN(
                "the task should process the message and respond to host "
                "comms") {
                REQUIRE(tasks->get_system_task().get_led_blink_state() ==
                        system_task::LEDBlinkState::BLINK_OFF);
                REQUIRE(tasks->get_system_queue().backing_deque.empty());
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
                auto response =
                    tasks->get_host_comms_queue().backing_deque.front();
                tasks->get_host_comms_queue().backing_deque.pop_front();
                REQUIRE(std::holds_alternative<messages::AcknowledgePrevious>(
                    response));
                auto getackresponse =
                    std::get<messages::AcknowledgePrevious>(response);
                REQUIRE(getackresponse.responding_to_id == message.id);
                REQUIRE(getackresponse.with_error ==
                        errors::ErrorCode::NO_ERROR);
            }
        }
    }
}
