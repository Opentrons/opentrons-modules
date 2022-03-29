#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/system_task.hpp"

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
                auto usb_ack = messages::AcknowledgePrevious{
                    .responding_to_id =
                        std::get<messages::ForceUSBDisconnectMessage>(
                            tasks->get_host_comms_queue().backing_deque.front())
                            .id};
                tasks->get_system_queue().backing_deque.push_front(usb_ack);
                THEN(
                    "the system should not enter the bootloader until all acks "
                    "are in") {
                    tasks->run_system_task();
                    REQUIRE(tasks->get_system_policy().bootloader_entered());
                }
            }
        }

        WHEN("calling button press callback") {
            tasks->get_system_task().front_button_callback();
            THEN("a Front Button message is sent to motor task") {
                REQUIRE(tasks->get_motor_queue().has_message());
                REQUIRE(
                    std::holds_alternative<messages::FrontButtonPressMessage>(
                        tasks->get_motor_queue().backing_deque.front()));
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

        WHEN("sending a set-led message") {
            auto message = messages::SetLedMode{.color = colors::Colors::BLUE,
                                                .mode = colors::Mode::SOLID};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            THEN("the task should get the message") {
                REQUIRE(tasks->get_system_queue().backing_deque.empty());
                AND_THEN("the task's LED status should be updated") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == message.mode);
                    REQUIRE(led.color == colors::get_color(message.color));
                }
            }
        }
        WHEN("sending an UpdateTaskErrorState message with a plate error") {
            auto message = messages::UpdateTaskErrorState{
                .task = messages::UpdateTaskErrorState::Tasks::THERMAL_PLATE,
                .current_error =
                    errors::ErrorCode::THERMISTOR_BACK_CENTER_OVERTEMP};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN("the task should set the LED outputs to the error mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::BLINKING);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::ORANGE));
                }
            }
        }
        WHEN("sending an UpdateTaskErrorState message with a lid error") {
            auto message = messages::UpdateTaskErrorState{
                .task = messages::UpdateTaskErrorState::Tasks::THERMAL_LID,
                .current_error =
                    errors::ErrorCode::THERMISTOR_LID_DISCONNECTED};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN("the task should set the LED outputs to the error mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::BLINKING);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::ORANGE));
                }
            }
        }
        WHEN("sending an UpdateTaskErrorState message with a motor error") {
            auto message = messages::UpdateTaskErrorState{
                .task = messages::UpdateTaskErrorState::Tasks::MOTOR,
                .current_error = errors::ErrorCode::SEAL_MOTOR_SPI_ERROR};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN("the task should set the LED outputs to the error mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::BLINKING);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::ORANGE));
                }
            }
        }
        WHEN("sending a UpdatePlateStatus message with an idle plate") {
            auto message = messages::UpdatePlateState{
                .state = messages::UpdatePlateState::PlateState::IDLE};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN("the task should set the LED outputs to the idle mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::SOLID);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::SOFT_WHITE));
                }
            }
        }
        WHEN("sending a UpdatePlateStatus message with a heating plate") {
            auto message = messages::UpdatePlateState{
                .state = messages::UpdatePlateState::PlateState::HEATING};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN(
                    "the task should set the LED outputs to the heating mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::PULSING);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::RED));
                }
            }
        }
        WHEN("sending a UpdatePlateStatus message with plate at hot temp") {
            auto message = messages::UpdatePlateState{
                .state = messages::UpdatePlateState::PlateState::AT_HOT_TEMP};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN(
                    "the task should set the LED outputs to the heating mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::SOLID);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::RED));
                }
            }
        }
        WHEN("sending a UpdatePlateStatus message with a cooling plate") {
            auto message = messages::UpdatePlateState{
                .state = messages::UpdatePlateState::PlateState::COOLING};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN(
                    "the task should set the LED outputs to the cooling mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::PULSING);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::BLUE));
                }
            }
        }
        WHEN("sending a UpdatePlateStatus message with plate at cool temp") {
            auto message = messages::UpdatePlateState{
                .state = messages::UpdatePlateState::PlateState::AT_COLD_TEMP};
            tasks->get_system_queue().backing_deque.push_back(
                messages::SystemMessage(message));
            tasks->get_system_task().run_once(tasks->get_system_policy());
            AND_WHEN("sending an LED update message") {
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(messages::UpdateUIMessage()));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN(
                    "the task should set the LED outputs to the cooling mode") {
                    auto &led = tasks->get_system_task().get_led_state();
                    REQUIRE(led.mode == colors::Mode::SOLID);
                    REQUIRE(led.color ==
                            colors::get_color(colors::Colors::BLUE));
                }
            }
        }
        GIVEN("front button pressed") {
            tasks->get_system_policy().set_front_button_status(true);
            WHEN("sending a GetFrontButton message") {
                auto message = messages::GetFrontButtonMessage{.id = 123};
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(message));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN("a response is sent to host comms with the correct data") {
                    REQUIRE(!tasks->get_system_queue().has_message());
                    auto host_message =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(std::holds_alternative<
                            messages::GetFrontButtonResponse>(host_message));
                    auto response = std::get<messages::GetFrontButtonResponse>(
                        host_message);
                    REQUIRE(response.responding_to_id == message.id);
                    REQUIRE(response.button_pressed);
                }
            }
        }
        GIVEN("front button not pressed") {
            tasks->get_system_policy().set_front_button_status(false);
            WHEN("sending a GetFrontButton message") {
                auto message = messages::GetFrontButtonMessage{.id = 123};
                tasks->get_system_queue().backing_deque.push_back(
                    messages::SystemMessage(message));
                tasks->get_system_task().run_once(tasks->get_system_policy());
                THEN("a response is sent to host comms with the correct data") {
                    REQUIRE(!tasks->get_system_queue().has_message());
                    auto host_message =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(std::holds_alternative<
                            messages::GetFrontButtonResponse>(host_message));
                    auto response = std::get<messages::GetFrontButtonResponse>(
                        host_message);
                    REQUIRE(response.responding_to_id == message.id);
                    REQUIRE(!response.button_pressed);
                }
            }
        }
    }
}