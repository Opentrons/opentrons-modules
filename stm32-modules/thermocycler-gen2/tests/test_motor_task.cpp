#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/motor_task.hpp"
#include "thermocycler-gen2/motor_utils.hpp"

SCENARIO("motor task message passing") {
    GIVEN("a motor task") {
        auto tasks = TaskBuilder::build();
        auto &motor_task = tasks->get_motor_task();
        auto &motor_policy = tasks->get_motor_policy();
        auto &motor_queue = tasks->get_motor_queue();

        THEN("the TMC2130 is not initialized") {
            REQUIRE(!motor_policy.has_been_written());
        }

        WHEN("sending ActuateSolenoid message to turn solenoid on") {
            auto message =
                messages::ActuateSolenoidMessage{.id = 123, .engage = true};
            motor_queue.backing_deque.push_back(message);
            tasks->run_motor_task();
            THEN(
                "the motor task should tun on the solenoid and respond to the "
                "message") {
                REQUIRE(motor_queue.backing_deque.empty());
                REQUIRE(!tasks->get_host_comms_queue().backing_deque.empty());
            }
            THEN("the TMC2130 has been initialized by the task") {
                REQUIRE(motor_policy.has_been_written());
            }
            THEN("the solenoid should be actuated") {
                REQUIRE(motor_policy.solenoid_engaged());
            }
            AND_THEN("sending ActuateSolenoid message to turn solenoid off") {
                message.id = 456;
                message.engage = false;
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the solenoid message is received") {
                    REQUIRE(motor_queue.backing_deque.empty());
                }
                THEN("the solenoid is disabled") {
                    REQUIRE(!motor_policy.solenoid_engaged());
                }
            }
        }
        WHEN("sending a LidStepperDebugMessage") {
            static constexpr double ANGLE = 10.0F;
            auto message = messages::LidStepperDebugMessage{
                .id = 123, .angle = ANGLE, .overdrive = true};
            motor_queue.backing_deque.push_back(message);
            tasks->run_motor_task();
            THEN("the message is received but no response is sent yet") {
                REQUIRE(motor_policy.get_lid_overdrive());
                REQUIRE(motor_policy.get_vref() > 0);
                REQUIRE(motor_policy.get_angle() ==
                        motor_util::LidStepper::angle_to_microsteps(ANGLE));
                REQUIRE(motor_queue.backing_deque.empty());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
            }
            AND_WHEN("sending another one") {
                message.id = 999;
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the second message is ACKed with an error") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto ack =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            ack));
                    auto ack_msg = std::get<messages::AcknowledgePrevious>(ack);
                    REQUIRE(ack_msg.responding_to_id == 999);
                    REQUIRE(ack_msg.with_error ==
                            errors::ErrorCode::LID_MOTOR_BUSY);
                }
            }
            AND_WHEN("receiving a LidStepperComplete message") {
                auto done_msg = messages::LidStepperComplete();
                motor_queue.backing_deque.push_back(done_msg);
                tasks->run_motor_task();
                THEN("the ACK is sent to the host comms task") {
                    REQUIRE(motor_policy.get_vref() == 0);
                    REQUIRE(motor_queue.backing_deque.empty());
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto ack =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            ack));
                    auto ack_msg = std::get<messages::AcknowledgePrevious>(ack);
                    REQUIRE(ack_msg.responding_to_id == 123);
                }
            }
            AND_WHEN("sending a GetLidStatus message") {
                auto message = messages::GetLidStatusMessage{.id = 123};
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the response shows the lid moving") {
                    auto msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    auto response =
                        std::get<messages::GetLidStatusResponse>(msg);
                    REQUIRE(response.lid ==
                            motor_util::LidStepper::Position::BETWEEN);
                }
            }
        }
        GIVEN("a lid motor fault") {
            motor_policy.trigger_lid_fault();
            WHEN("sending a LidStepperDebugMessage") {
                static constexpr double ANGLE = 10.0F;
                auto message =
                    messages::LidStepperDebugMessage{.id = 123, .angle = ANGLE};
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the message is ACKed with an error") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto ack =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            ack));
                    auto ack_msg = std::get<messages::AcknowledgePrevious>(ack);
                    REQUIRE(ack_msg.responding_to_id == 123);
                    REQUIRE(ack_msg.with_error ==
                            errors::ErrorCode::LID_MOTOR_FAULT);
                }
                THEN("no motion is started") {
                    REQUIRE(motor_policy.get_vref() == 0);
                }
            }
        }
        WHEN("sending a SealStepperDebugMessage with positive steps") {
            static constexpr int32_t STEPS = 10;
            auto message =
                messages::SealStepperDebugMessage{.id = 123, .steps = STEPS};
            motor_queue.backing_deque.push_back(message);
            tasks->run_motor_task();
            THEN("the message is received but no response is sent yet") {
                REQUIRE(motor_policy.seal_moving());
                // True for positive
                REQUIRE(motor_policy.get_tmc2130_direction());
                REQUIRE(motor_policy.get_tmc2130_enabled());
                REQUIRE(motor_queue.backing_deque.empty());
                REQUIRE(tasks->get_host_comms_queue().backing_deque.empty());
            }
            AND_WHEN("sending another one") {
                message.id = 999;
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the second message is ACKed with an error") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto ack =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            ack));
                    auto ack_msg = std::get<messages::AcknowledgePrevious>(ack);
                    REQUIRE(ack_msg.responding_to_id == 999);
                    REQUIRE(ack_msg.with_error ==
                            errors::ErrorCode::SEAL_MOTOR_BUSY);
                }
            }
            WHEN("sending a SealStepperComplete message with a stall") {
                using namespace messages;
                auto stall_msg = SealStepperComplete{
                    .reason = SealStepperComplete::CompletionReason::STALL};
                motor_queue.backing_deque.push_back(stall_msg);
                tasks->run_motor_task();
                THEN(
                    "the original message is ACK'd with a reduced step count "
                    "and no error") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto ack =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(std::holds_alternative<
                            messages::SealStepperDebugResponse>(ack));
                    auto ack_msg =
                        std::get<messages::SealStepperDebugResponse>(ack);
                    REQUIRE(ack_msg.responding_to_id == 123);
                    REQUIRE(ack_msg.steps_taken == 0);
                    REQUIRE(ack_msg.with_error == errors::ErrorCode::NO_ERROR);
                }
                THEN("the seal motor was stopped") {
                    REQUIRE(!motor_policy.seal_moving());
                }
            }
            WHEN("sending a SealStepperComplete message with an error") {
                using namespace messages;
                auto stall_msg = SealStepperComplete{
                    .reason = SealStepperComplete::CompletionReason::ERROR};
                motor_queue.backing_deque.push_back(stall_msg);
                tasks->run_motor_task();
                THEN("the original message is ACK'd with an error") {
                    REQUIRE(
                        !tasks->get_host_comms_queue().backing_deque.empty());
                    auto ack =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(std::holds_alternative<
                            messages::SealStepperDebugResponse>(ack));
                    auto ack_msg =
                        std::get<messages::SealStepperDebugResponse>(ack);
                    REQUIRE(ack_msg.responding_to_id == 123);
                    REQUIRE(ack_msg.steps_taken == 0);
                    REQUIRE(ack_msg.with_error ==
                            errors::ErrorCode::SEAL_MOTOR_FAULT);
                }
                THEN("the seal motor was stopped") {
                    REQUIRE(!motor_policy.seal_moving());
                }
            }
            AND_WHEN("sending a GetLidStatus message") {
                auto message = messages::GetLidStatusMessage{.id = 123};
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the response shows the seal moving") {
                    auto msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    auto response =
                        std::get<messages::GetLidStatusResponse>(msg);
                    REQUIRE(response.seal ==
                            motor_util::SealStepper::Status::BETWEEN);
                }
            }
            AND_WHEN("incrementing the tick for up to a second") {
                uint32_t i = 0;
                for (; i < motor_policy.MotorTickFrequency; ++i) {
                    motor_policy.tick();
                    if (!motor_policy.seal_moving()) {
                        break;
                    }
                }
                THEN("the seal movement ends after at least 1 second") {
                    REQUIRE(!motor_policy.seal_moving());
                    REQUIRE(i >= 10);
                }
                THEN("the seal motor has moved 10 steps") {
                    REQUIRE(motor_policy.get_tmc2130_steps() == 10);
                }
                THEN("a SealStepperComplete message is received") {
                    REQUIRE(!motor_queue.backing_deque.empty());
                    auto msg = motor_queue.backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::SealStepperComplete>(
                            msg));
                }
                WHEN("running the task") {
                    tasks->run_motor_task();
                    THEN("an ack is received by the host task") {
                        REQUIRE(motor_queue.backing_deque.empty());
                        REQUIRE(!tasks->get_host_comms_queue()
                                     .backing_deque.empty());
                        auto ack =
                            tasks->get_host_comms_queue().backing_deque.front();
                        REQUIRE(std::holds_alternative<
                                messages::SealStepperDebugResponse>(ack));
                        auto ack_msg =
                            std::get<messages::SealStepperDebugResponse>(ack);
                        REQUIRE(ack_msg.responding_to_id == 123);
                        REQUIRE(ack_msg.steps_taken == STEPS);
                        REQUIRE(ack_msg.with_error ==
                                errors::ErrorCode::NO_ERROR);
                    }
                }
            }
        }
        WHEN("sending a SealStepperDebugMessage with negative steps") {
            static constexpr int32_t STEPS = -10;
            auto message =
                messages::SealStepperDebugMessage{.id = 123, .steps = STEPS};
            motor_queue.backing_deque.push_back(message);
            tasks->run_motor_task();
            AND_WHEN("incrementing the tick for up to a second") {
                uint32_t i = 0;
                for (; i < motor_policy.MotorTickFrequency; ++i) {
                    motor_policy.tick();
                    if (!motor_policy.seal_moving()) {
                        break;
                    }
                }
                THEN("a SealStepperComplete message is received") {
                    REQUIRE(!motor_queue.backing_deque.empty());
                    auto msg = motor_queue.backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::SealStepperComplete>(
                            msg));
                }
                WHEN("running the task") {
                    tasks->run_motor_task();
                    THEN("an ack is received by the host task") {
                        REQUIRE(motor_queue.backing_deque.empty());
                        REQUIRE(!tasks->get_host_comms_queue()
                                     .backing_deque.empty());
                        auto ack =
                            tasks->get_host_comms_queue().backing_deque.front();
                        REQUIRE(std::holds_alternative<
                                messages::SealStepperDebugResponse>(ack));
                        auto ack_msg =
                            std::get<messages::SealStepperDebugResponse>(ack);
                        REQUIRE(ack_msg.responding_to_id == 123);
                        REQUIRE(ack_msg.steps_taken == STEPS);
                        REQUIRE(ack_msg.with_error ==
                                errors::ErrorCode::NO_ERROR);
                    }
                }
            }
        }
        WHEN(
            "sending a GetSealDriveStatus command with a stallguard result of "
            "0xF") {
            motor_policy.write_register(tmc2130::Registers::DRVSTATUS, 0xF);
            auto message = messages::GetSealDriveStatusMessage{.id = 123};
            motor_queue.backing_deque.push_back(message);
            tasks->run_motor_task();
            THEN("the message is recieved and ACKd with correct data") {
                REQUIRE(!motor_queue.has_message());
                REQUIRE(tasks->get_host_comms_queue().has_message());
                auto msg = tasks->get_host_comms_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<
                        messages::GetSealDriveStatusResponse>(msg));
                auto response =
                    std::get<messages::GetSealDriveStatusResponse>(msg);
                REQUIRE(response.responding_to_id == message.id);
                REQUIRE(response.status.sg_result == 0xF);
                REQUIRE(response.status.stallguard == 0);
            }
        }
        WHEN("sending a SetSealParameter message with hold current = 0x2F") {
            auto message = messages::SetSealParameterMessage{
                .id = 123,
                .param = motor_util::SealStepper::Parameter::HoldCurrent,
                .value = 1000};
            motor_queue.backing_deque.push_back(message);
            tasks->run_motor_task();
            THEN("the message is recieved and ACKd with correct id") {
                REQUIRE(!motor_queue.has_message());
                REQUIRE(tasks->get_host_comms_queue().has_message());
                auto msg = tasks->get_host_comms_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::AcknowledgePrevious>(msg));
                auto response = std::get<messages::AcknowledgePrevious>(msg);
                REQUIRE(response.responding_to_id == message.id);
                REQUIRE(response.with_error == errors::ErrorCode::NO_ERROR);
            }
            THEN("the current is set to a clamped value of 0x1F (5 bits)") {
                auto reg =
                    motor_policy.read_register(tmc2130::Registers::IHOLD_IRUN);
                uint32_t mask = 0x1F;
                REQUIRE(reg.has_value());
                REQUIRE((reg.value() & mask) == mask);
            }
        }
        WHEN("sending a GetLidStatus message") {
            auto message = messages::GetLidStatusMessage{.id = 123};
            motor_queue.backing_deque.push_back(message);
            tasks->run_motor_task();
            THEN("the message is received and responded to") {
                REQUIRE(!motor_queue.has_message());
                REQUIRE(tasks->get_host_comms_queue().has_message());
                auto msg = tasks->get_host_comms_queue().backing_deque.front();
                REQUIRE(std::holds_alternative<messages::GetLidStatusResponse>(
                    msg));
                auto response = std::get<messages::GetLidStatusResponse>(msg);
                REQUIRE(response.responding_to_id == message.id);
                REQUIRE(response.lid ==
                        motor_util::LidStepper::Position::BETWEEN);
                REQUIRE(response.seal ==
                        motor_util::SealStepper::Status::UNKNOWN);
            }
        }
        GIVEN("triggered lid close switch") {
            tasks->get_motor_policy().set_lid_closed_switch(true);
            WHEN("sending a GetLidStatus message") {
                auto message = messages::GetLidStatusMessage{.id = 123};
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the response shows the lid closed") {
                    auto msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    auto response =
                        std::get<messages::GetLidStatusResponse>(msg);
                    REQUIRE(response.lid ==
                            motor_util::LidStepper::Position::CLOSED);
                }
            }
        }
        GIVEN("triggered lid open switch") {
            tasks->get_motor_policy().set_lid_open_switch(true);
            WHEN("sending a GetLidStatus message") {
                auto message = messages::GetLidStatusMessage{.id = 123};
                motor_queue.backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN("the response shows the lid open") {
                    auto msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    auto response =
                        std::get<messages::GetLidStatusResponse>(msg);
                    REQUIRE(response.lid ==
                            motor_util::LidStepper::Position::OPEN);
                }
            }
        }
        WHEN("sending OpenLid command") {
            tasks->get_motor_queue().backing_deque.push_back(
                messages::OpenLidMessage{.id = 123});
            tasks->run_motor_task();
            THEN("the lid starts opening") {
                REQUIRE(
                    motor_task.get_lid_state() ==
                    motor_task::LidState::Status::OPENING_PARTIAL_EXTEND_SEAL);
            }
            AND_WHEN("sending another OpenLid command immediately") {
                tasks->get_motor_queue().backing_deque.push_back(
                    messages::OpenLidMessage{.id = 456});
                tasks->run_motor_task();
                THEN("the second command is ignored with an error") {
                    REQUIRE(tasks->get_host_comms_queue().has_message());
                    auto msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    auto reply_msg =
                        std::get<messages::AcknowledgePrevious>(msg);
                    REQUIRE(reply_msg.responding_to_id == 456);
                    REQUIRE(reply_msg.with_error ==
                            errors::ErrorCode::LID_MOTOR_BUSY);
                }
            }
        }
        WHEN("sending CloseLid command") {
            tasks->get_motor_queue().backing_deque.push_back(
                messages::CloseLidMessage{.id = 123});
            tasks->run_motor_task();
            THEN("the lid starts moving to the endstop") {
                REQUIRE(
                    motor_task.get_lid_state() ==
                    motor_task::LidState::Status::CLOSING_PARTIAL_EXTEND_SEAL);
            }
            AND_WHEN("sending another CloseLid command immediately") {
                tasks->get_motor_queue().backing_deque.push_back(
                    messages::CloseLidMessage{.id = 456});
                tasks->run_motor_task();
                THEN("the second command is ignored with an error") {
                    REQUIRE(tasks->get_host_comms_queue().has_message());
                    auto msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    auto reply_msg =
                        std::get<messages::AcknowledgePrevious>(msg);
                    REQUIRE(reply_msg.responding_to_id == 456);
                    REQUIRE(reply_msg.with_error ==
                            errors::ErrorCode::LID_MOTOR_BUSY);
                }
            }
        }
        WHEN("sending a Front Button message with lid in unknown position") {
            tasks->get_motor_queue().backing_deque.push_back(
                messages::FrontButtonPressMessage());
            tasks->run_motor_task();
            THEN("the lid starts to open") {
                REQUIRE(
                    motor_task.get_lid_state() ==
                    motor_task::LidState::Status::OPENING_PARTIAL_EXTEND_SEAL);
            }
        }
        GIVEN("lid closed sensor triggered") {
            motor_policy.set_lid_closed_switch(true);
            motor_policy.set_lid_open_switch(false);
            WHEN("sending a Front Button message") {
                tasks->get_motor_queue().backing_deque.push_back(
                    messages::FrontButtonPressMessage());
                tasks->run_motor_task();
                THEN("the lid starts to open") {
                    REQUIRE(motor_task.get_lid_state() ==
                            motor_task::LidState::Status::OPENING_RETRACT_SEAL);
                }
            }
            WHEN("sending a GetLidSwitches message") {
                auto message = messages::GetLidSwitchesMessage{.id = 123};
                tasks->get_motor_queue().backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN(
                    "a response is sent to host comms with the correct "
                    "data") {
                    REQUIRE(!tasks->get_motor_queue().has_message());
                    auto host_message =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(std::holds_alternative<
                            messages::GetLidSwitchesResponse>(host_message));
                    auto response = std::get<messages::GetLidSwitchesResponse>(
                        host_message);
                    REQUIRE(response.responding_to_id == message.id);
                    REQUIRE(response.close_switch_pressed);
                    REQUIRE(!response.open_switch_pressed);
                }
            }
        }
        GIVEN("lid open sensor triggered") {
            motor_policy.set_lid_open_switch(true);
            motor_policy.set_lid_closed_switch(false);
            WHEN("sending a Front Button message") {
                tasks->get_motor_queue().backing_deque.push_back(
                    messages::FrontButtonPressMessage());
                tasks->run_motor_task();
                THEN("the lid starts to close") {
                    REQUIRE(motor_task.get_lid_state() ==
                            motor_task::LidState::Status::
                                CLOSING_PARTIAL_EXTEND_SEAL);
                }
            }
            WHEN("sending a GetLidSwitches message") {
                auto message = messages::GetLidSwitchesMessage{.id = 123};
                tasks->get_motor_queue().backing_deque.push_back(message);
                tasks->run_motor_task();
                THEN(
                    "a response is sent to host comms with the correct "
                    "data") {
                    REQUIRE(!tasks->get_motor_queue().has_message());
                    auto host_message =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(std::holds_alternative<
                            messages::GetLidSwitchesResponse>(host_message));
                    auto response = std::get<messages::GetLidSwitchesResponse>(
                        host_message);
                    REQUIRE(response.responding_to_id == message.id);
                    REQUIRE(!response.close_switch_pressed);
                    REQUIRE(response.open_switch_pressed);
                }
            }
        }
    }
}

SCENARIO("motor task open and close lid behavior") {
    auto tasks = TaskBuilder::build();
    auto &motor_task = tasks->get_motor_task();
    auto &motor_policy = tasks->get_motor_policy();
    auto &motor_queue = tasks->get_motor_queue();
    GIVEN("lid closed on startup") {
        motor_policy.set_lid_closed_switch(true);
        motor_policy.set_lid_open_switch(false);
        WHEN("sending open lid command") {
            motor_queue.backing_deque.push_back(
                messages::OpenLidMessage{.id = 123});
            tasks->run_motor_task();
            THEN("the seal starts retracting") {
                REQUIRE(motor_policy.seal_moving());
                REQUIRE(
                    motor_policy
                        .get_tmc2130_direction());  // Retracting is positive
            }
            auto lid_position = motor_policy.get_angle();
            WHEN("seal movement ends with a stall") {
                motor_queue.backing_deque.push_back(
                    messages::SealStepperComplete{
                        .reason = messages::SealStepperComplete::
                            CompletionReason::STALL});
                tasks->run_motor_task();
                THEN("the seal stops moving") {
                    REQUIRE(!motor_policy.seal_moving());
                }
                THEN("the seal is in the Retracted position") {
                    REQUIRE(motor_task.get_seal_position() ==
                            motor_util::SealStepper::Status::RETRACTED);
                }
                THEN("the lid starts opening") {
                    REQUIRE(motor_policy.get_angle() > lid_position);
                }
                WHEN("lid movement completes") {
                    motor_queue.backing_deque.push_back(
                        messages::LidStepperComplete());
                    lid_position = motor_policy.get_angle();
                    tasks->run_motor_task();
                    THEN("the lid overdrives into the switch a bit") {
                        REQUIRE(motor_policy.get_angle() > lid_position);
                        REQUIRE(motor_policy.get_lid_overdrive());
                    }
                    WHEN("the lid movement completes") {
                        motor_queue.backing_deque.push_back(
                            messages::LidStepperComplete());
                        tasks->run_motor_task();
                        THEN("the command is acknowledged") {
                            REQUIRE(
                                tasks->get_host_comms_queue().has_message());
                            auto msg = tasks->get_host_comms_queue()
                                           .backing_deque.front();
                            REQUIRE(std::holds_alternative<
                                    messages::AcknowledgePrevious>(msg));
                            auto response =
                                std::get<messages::AcknowledgePrevious>(msg);
                            REQUIRE(response.responding_to_id == 123);
                            REQUIRE(response.with_error ==
                                    errors::ErrorCode::NO_ERROR);
                        }
                    }
                }
            }
        }
        WHEN("sending a lid close command") {
            motor_queue.backing_deque.push_back(
                messages::CloseLidMessage{.id = 456});
            tasks->run_motor_task();
            THEN("the message is immediately acked") {
                REQUIRE(tasks->get_host_comms_queue().has_message());
                auto msg = tasks->get_host_comms_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::AcknowledgePrevious>(msg));
                auto response = std::get<messages::AcknowledgePrevious>(msg);
                REQUIRE(response.responding_to_id == 456);
                REQUIRE(response.with_error == errors::ErrorCode::NO_ERROR);
            }
        }
    }
    GIVEN("lid unknown at startup") {
        motor_policy.set_lid_closed_switch(false);
        motor_policy.set_lid_open_switch(false);
        REQUIRE(motor_task.get_seal_position() ==
                motor_util::SealStepper::Status::UNKNOWN);
        WHEN("sending plate lift command") {
            motor_queue.backing_deque.push_back(
                messages::PlateLiftMessage{.id = 123});
            tasks->run_motor_task();
            THEN("an error is returned") {
                REQUIRE(tasks->get_host_comms_queue().has_message());
                auto msg = tasks->get_host_comms_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::AcknowledgePrevious>(msg));
                auto response = std::get<messages::AcknowledgePrevious>(msg);
                REQUIRE(response.responding_to_id == 123);
                REQUIRE(response.with_error == errors::ErrorCode::LID_CLOSED);
            }
            THEN("no movement occurs") {
                REQUIRE(motor_task.get_lid_state() ==
                        motor_task::LidState::Status::IDLE);
            }
        }
        WHEN("sending open lid command") {
            motor_queue.backing_deque.push_back(
                messages::OpenLidMessage{.id = 123});
            tasks->run_motor_task();
            THEN("the seal starts to extend") {
                REQUIRE(motor_policy.seal_moving());
                REQUIRE(
                    !motor_policy
                         .get_tmc2130_direction());  // Retracting is positive
            }
            WHEN("seal movement ends without a stall") {
                motor_queue.backing_deque.push_back(
                    messages::SealStepperComplete());
                tasks->run_motor_task();
                THEN("the seal starts retracting") {
                    REQUIRE(motor_policy.seal_moving());
                    REQUIRE(
                        motor_policy.get_tmc2130_direction());  // Retracting is
                                                                // positive
                }
                auto lid_position = motor_policy.get_angle();
                WHEN("seal movement ends with a stall") {
                    motor_queue.backing_deque.push_back(
                        messages::SealStepperComplete{
                            .reason = messages::SealStepperComplete::
                                CompletionReason::STALL});
                    tasks->run_motor_task();
                    THEN("the seal stops moving") {
                        REQUIRE(!motor_policy.seal_moving());
                    }
                    THEN("the seal is in the Retracted position") {
                        REQUIRE(motor_task.get_seal_position() ==
                                motor_util::SealStepper::Status::RETRACTED);
                    }
                    THEN("the lid starts opening") {
                        REQUIRE(motor_policy.get_angle() > lid_position);
                    }
                    WHEN("lid movement completes") {
                        motor_queue.backing_deque.push_back(
                            messages::LidStepperComplete());
                        lid_position = motor_policy.get_angle();
                        tasks->run_motor_task();
                        THEN("the lid overdrives into the switch a bit") {
                            REQUIRE(motor_policy.get_angle() > lid_position);
                            REQUIRE(motor_policy.get_lid_overdrive());
                        }
                        WHEN("the lid movement completes") {
                            motor_queue.backing_deque.push_back(
                                messages::LidStepperComplete());
                            tasks->run_motor_task();
                            THEN("the command is acknowledged") {
                                REQUIRE(tasks->get_host_comms_queue()
                                            .has_message());
                                auto msg = tasks->get_host_comms_queue()
                                               .backing_deque.front();
                                REQUIRE(std::holds_alternative<
                                        messages::AcknowledgePrevious>(msg));
                                auto response =
                                    std::get<messages::AcknowledgePrevious>(
                                        msg);
                                REQUIRE(response.responding_to_id == 123);
                                REQUIRE(response.with_error ==
                                        errors::ErrorCode::NO_ERROR);
                            }
                        }
                    }
                }
            }
        }
        WHEN("sending a lid close command") {
            motor_queue.backing_deque.push_back(
                messages::CloseLidMessage{.id = 456});
            tasks->run_motor_task();

            THEN("the seal starts to extend") {
                REQUIRE(motor_policy.seal_moving());
                REQUIRE(
                    !motor_policy
                         .get_tmc2130_direction());  // Retracting is positive
            }
            WHEN("seal movement ends without a stall") {
                motor_queue.backing_deque.push_back(
                    messages::SealStepperComplete());
                tasks->run_motor_task();
                THEN("the seal starts retracting") {
                    REQUIRE(motor_policy.seal_moving());
                    REQUIRE(
                        motor_policy.get_tmc2130_direction());  // Retracting is
                                                                // positive
                }
                auto lid_position = motor_policy.get_angle();
                WHEN("seal movement ends with a stall") {
                    motor_queue.backing_deque.push_back(
                        messages::SealStepperComplete{
                            .reason = messages::SealStepperComplete::
                                CompletionReason::STALL});
                    tasks->run_motor_task();
                    THEN("the seal stops moving") {
                        REQUIRE(!motor_policy.seal_moving());
                    }
                    THEN("the seal is in the Retracted position") {
                        REQUIRE(motor_task.get_seal_position() ==
                                motor_util::SealStepper::Status::RETRACTED);
                    }
                    THEN("the lid starts closing") {
                        REQUIRE(motor_policy.get_angle() < lid_position);
                    }
                    WHEN("lid movement completes") {
                        motor_queue.backing_deque.push_back(
                            messages::LidStepperComplete());
                        lid_position = motor_policy.get_angle();
                        tasks->run_motor_task();
                        THEN("the lid overdrives into the switch") {
                            REQUIRE(motor_policy.get_angle() < lid_position);
                            REQUIRE(motor_policy.get_lid_overdrive());
                        }
                        WHEN("the lid movement completes") {
                            motor_queue.backing_deque.push_back(
                                messages::LidStepperComplete());
                            tasks->run_motor_task();
                            THEN("the seal is engaged") {
                                REQUIRE(motor_policy.seal_moving());
                                REQUIRE(
                                    !motor_policy
                                         .get_tmc2130_direction());  // Retracting
                                                                     // is
                                                                     // positive
                            }
                            AND_WHEN("the seal movement ends") {
                                motor_queue.backing_deque.push_back(
                                    messages::SealStepperComplete());
                                tasks->run_motor_task();
                                REQUIRE(
                                    motor_task.get_seal_position() ==
                                    motor_util::SealStepper::Status::ENGAGED);
                                THEN("the command is acknowledged") {
                                    REQUIRE(tasks->get_host_comms_queue()
                                                .has_message());
                                    auto msg = tasks->get_host_comms_queue()
                                                   .backing_deque.front();
                                    REQUIRE(std::holds_alternative<
                                            messages::AcknowledgePrevious>(
                                        msg));
                                    auto response =
                                        std::get<messages::AcknowledgePrevious>(
                                            msg);
                                    REQUIRE(response.responding_to_id == 456);
                                    REQUIRE(response.with_error ==
                                            errors::ErrorCode::NO_ERROR);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    GIVEN("lid open on startup") {
        motor_policy.set_lid_closed_switch(false);
        motor_policy.set_lid_open_switch(true);
        WHEN("sending open lid command") {
            motor_queue.backing_deque.push_back(
                messages::OpenLidMessage{.id = 123});
            tasks->run_motor_task();
            THEN("an ACK is returned") {
                REQUIRE(tasks->get_host_comms_queue().has_message());
                auto msg = tasks->get_host_comms_queue().backing_deque.front();
                REQUIRE(
                    std::holds_alternative<messages::AcknowledgePrevious>(msg));
                auto response = std::get<messages::AcknowledgePrevious>(msg);
                REQUIRE(response.responding_to_id == 123);
                REQUIRE(response.with_error == errors::ErrorCode::NO_ERROR);
            }
            THEN("no movement occurs") {
                REQUIRE(motor_task.get_lid_state() ==
                        motor_task::LidState::Status::IDLE);
            }
        }
        WHEN("sending plate lift command") {
            motor_queue.backing_deque.push_back(
                messages::PlateLiftMessage{.id = 123});
            auto lid_angle = motor_policy.get_angle();
            tasks->run_motor_task();
            THEN("the lid opens further") {
                REQUIRE(motor_policy.get_angle() > lid_angle);
                REQUIRE(motor_policy.get_lid_overdrive());
                REQUIRE(motor_task.get_lid_state() ==
                        motor_task::LidState::Status::PLATE_LIFTING);
                AND_WHEN("the lid movement ends") {
                    motor_queue.backing_deque.push_back(
                        messages::LidStepperComplete());
                    tasks->run_motor_task();
                    THEN("the lid moves back down past the switch") {
                        // Compare to the ORIGINAL angle
                        REQUIRE(motor_policy.get_angle() < lid_angle);
                        REQUIRE(motor_policy.get_lid_overdrive());
                        AND_WHEN("the lid movement ends") {
                            motor_queue.backing_deque.push_back(
                                messages::LidStepperComplete());
                            tasks->run_motor_task();
                            THEN("the lid moves back to the switch") {
                                // Compare to the ORIGINAL angle
                                REQUIRE(motor_policy.get_angle() > lid_angle);
                                REQUIRE(!motor_policy.get_lid_overdrive());
                                lid_angle = motor_policy.get_angle();
                                AND_WHEN("the lid movement ends") {
                                    motor_queue.backing_deque.push_back(
                                        messages::LidStepperComplete());
                                    tasks->run_motor_task();
                                    THEN("the lid overdrives into the switch") {
                                        REQUIRE(motor_policy.get_angle() >
                                                lid_angle);
                                        REQUIRE(
                                            motor_policy.get_lid_overdrive());
                                        AND_WHEN("the final lid motion ends") {
                                            motor_queue.backing_deque.push_back(
                                                messages::LidStepperComplete());
                                            tasks->run_motor_task();
                                            THEN("an ACK is sent") {
                                                REQUIRE(motor_task
                                                            .get_lid_state() ==
                                                        motor_task::LidState::
                                                            Status::IDLE);
                                                REQUIRE(
                                                    tasks
                                                        ->get_host_comms_queue()
                                                        .has_message());
                                                auto msg =
                                                    tasks
                                                        ->get_host_comms_queue()
                                                        .backing_deque.front();
                                                REQUIRE(std::holds_alternative<
                                                        messages::
                                                            AcknowledgePrevious>(
                                                    msg));
                                                auto response = std::get<
                                                    messages::
                                                        AcknowledgePrevious>(
                                                    msg);
                                                REQUIRE(
                                                    response.responding_to_id ==
                                                    123);
                                                REQUIRE(response.with_error ==
                                                        errors::ErrorCode::
                                                            NO_ERROR);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
