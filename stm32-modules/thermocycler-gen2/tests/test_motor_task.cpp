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
                    REQUIRE(std::holds_alternative<
                            messages::SealStepperDebugResponse>(ack));
                    auto ack_msg =
                        std::get<messages::SealStepperDebugResponse>(ack);
                    REQUIRE(ack_msg.responding_to_id == 999);
                    REQUIRE(ack_msg.steps_taken == 0);
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
                REQUIRE(motor_task.get_lid_state() ==
                        motor_task::LidState::Status::OPENING_RETRACT_SEAL);
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
                REQUIRE(motor_task.get_lid_state() ==
                        motor_task::LidState::Status::CLOSING_RETRACT_SEAL);
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
                REQUIRE(motor_task.get_lid_state() ==
                        motor_task::LidState::Status::OPENING_RETRACT_SEAL);
            }
        }
        GIVEN("lid closed sensor triggered") {
            motor_policy.set_lid_closed_switch(true);
            motor_policy.set_lid_open_switch(false);
            motor_policy.set_seal_switch_triggered(false);
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
                    REQUIRE(!response.seal_switch_pressed);
                }
            }
        }
        GIVEN("lid open sensor triggered") {
            motor_policy.set_lid_open_switch(true);
            motor_policy.set_lid_closed_switch(false);
            motor_policy.set_seal_switch_triggered(false);
            WHEN("sending a Front Button message") {
                tasks->get_motor_queue().backing_deque.push_back(
                    messages::FrontButtonPressMessage());
                tasks->run_motor_task();
                THEN("the lid starts to close") {
                    REQUIRE(motor_task.get_lid_state() ==
                            motor_task::LidState::Status::CLOSING_RETRACT_SEAL);
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
                    REQUIRE(!response.seal_switch_pressed);
                }
            }
            GIVEN("seal sensor triggered") {
                motor_policy.set_seal_switch_triggered(true);
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
                                messages::GetLidSwitchesResponse>(
                            host_message));
                        auto response =
                            std::get<messages::GetLidSwitchesResponse>(
                                host_message);
                        REQUIRE(response.responding_to_id == message.id);
                        REQUIRE(!response.close_switch_pressed);
                        REQUIRE(response.open_switch_pressed);
                        REQUIRE(response.seal_switch_pressed);
                    }
                }
            }
        }
    }
}

struct MotorStep {
    // Message to send on this step
    messages::MotorMessage msg;
    // If true, expect that the lid angle increased after this message
    bool lid_angle_increased = false;
    // If true, expect that the lid angle decreased after this message
    bool lid_angle_decreased = false;
    // If the lid moved (increase or decrease), check that the limit switches
    // are either ignored or checked
    bool lid_overdrive = false;
    // Expected seal motor state
    bool seal_on = false;
    // Retraction is true, extension is false
    bool seal_direction = false;
    // If true, seal switch should be armed. If nullopt, doesn't matter.
    std::optional<bool> seal_switch_armed = std::nullopt;

    using SealPos = motor_util::SealStepper::Status;
    // If this variable is set, expect seal in a specific position
    std::optional<SealPos> seal_pos = std::nullopt;
    // If true, expect an ack in the host comms task
    std::optional<messages::AcknowledgePrevious> ack = std::nullopt;
};

/**
 * @brief Executes a set of steps to exercise the lid motor state machine.
 *
 * @pre The state of any switches should be set \e before invoking this
 */
void test_motor_state_machine(std::shared_ptr<TaskBuilder> tasks,
                              std::vector<MotorStep> &steps) {
    auto &motor_task = tasks->get_motor_task();
    auto &motor_policy = tasks->get_motor_policy();
    auto &motor_queue = tasks->get_motor_queue();
    for (size_t i = 0; i < steps.size(); ++i) {
        auto &step = steps[i];

        auto lid_angle_before = motor_policy.get_angle();
        motor_queue.backing_deque.push_back(step.msg);
        tasks->run_motor_task();

        DYNAMIC_SECTION("Step " << i) {
            if (step.lid_angle_increased) {
                THEN("the lid motor opened") {
                    REQUIRE(motor_policy.get_angle() > lid_angle_before);
                }
            }
            if (step.lid_angle_decreased) {
                THEN("the lid motor closed") {
                    REQUIRE(motor_policy.get_angle() < lid_angle_before);
                }
            }
            THEN("the seal is controlled correctly") {
                REQUIRE(motor_policy.seal_moving() == step.seal_on);
                if (step.seal_on) {
                    REQUIRE(motor_policy.get_tmc2130_direction() ==
                            step.seal_direction);
                }
            }
            if (step.seal_pos.has_value()) {
                THEN("the seal position is set correctly") {
                    REQUIRE(motor_task.get_seal_position() ==
                            step.seal_pos.value());
                }
            }
            if (step.ack.has_value()) {
                THEN("an ack is sent to host comms") {
                    auto ack = step.ack.value();
                    REQUIRE(tasks->get_host_comms_queue().has_message());
                    auto msg =
                        tasks->get_host_comms_queue().backing_deque.front();
                    REQUIRE(
                        std::holds_alternative<messages::AcknowledgePrevious>(
                            msg));
                    auto response =
                        std::get<messages::AcknowledgePrevious>(msg);
                    REQUIRE(response.responding_to_id == ack.responding_to_id);
                    REQUIRE(response.with_error == ack.with_error);
                }
            }
        }
    }
}

SCENARIO("motor task lid state machine") {
    auto tasks = TaskBuilder::build();
    auto &motor_policy = tasks->get_motor_policy();
    GIVEN("lid is closed on startup") {
        motor_policy.set_lid_closed_switch(true);
        motor_policy.set_lid_open_switch(false);
        WHEN("sending open lid command") {
            std::vector<MotorStep> steps = {
                // First step retracts seal switch
                {.msg = messages::OpenLidMessage{.id = 123},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = true},
                // Second step extends seeal switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false},
                // Third step opens hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_increased = true,
                 .lid_overdrive = false},
                // Fourth step overdrives hinge
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_increased = true,
                 .lid_overdrive = true},
                // Should send ACK now
                {.msg = messages::LidStepperComplete(),
                 .ack =
                     messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::NO_ERROR}},
            };
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending close lid command") {
            std::vector<MotorStep> steps = {
                // Command should end immediately
                {.msg = messages::CloseLidMessage{.id = 123},
                 .ack = messages::AcknowledgePrevious{
                     .responding_to_id = 123,
                     .with_error = errors::ErrorCode::NO_ERROR}}};
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending plate lift command") {
            std::vector<MotorStep> steps = {
                // Command should end with error
                {.msg = messages::PlateLiftMessage{.id = 123},
                 .ack = messages::AcknowledgePrevious{
                     .responding_to_id = 123,
                     .with_error = errors::ErrorCode::LID_CLOSED}}};
            test_motor_state_machine(tasks, steps);
        }
        GIVEN("seal limit switch is triggered") {
            motor_policy.set_seal_switch_triggered(true);
            WHEN("sending open lid command") {
                std::vector<MotorStep> steps = {
                    {.msg = messages::OpenLidMessage{.id = 123},
                     .ack = messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::SEAL_MOTOR_SWITCH}}};
                test_motor_state_machine(tasks, steps);
            }
        }
    }
    GIVEN("lid is open on startup") {
        motor_policy.set_lid_closed_switch(false);
        motor_policy.set_lid_open_switch(true);
        WHEN("sending open lid command") {
            std::vector<MotorStep> steps = {
                // No action
                {.msg = messages::OpenLidMessage{.id = 123},
                 .ack =
                     messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::NO_ERROR}},
            };
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending close lid command") {
            std::vector<MotorStep> steps = {
                // First step retracts seal to switch
                {.msg = messages::CloseLidMessage{.id = 123},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = true},
                // Second step extends seal from switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false},
                // Third step closes hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_decreased = true,
                 .lid_overdrive = false},
                // Fourth step overdrives hinge
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_decreased = true,
                 .lid_overdrive = true},
                // Now extend seal to switch
                {.msg = messages::LidStepperComplete(),
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = true},
                // Retract seal from switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = false},
                // Should send ACK now
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .ack =
                     messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::NO_ERROR}},
            };
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending plate lift command") {
            std::vector<MotorStep> steps = {
                // First open past the switch
                {.msg = messages::PlateLiftMessage{.id = 123},
                 .lid_angle_increased = true,
                 .lid_overdrive = true},
                // Now close back below the switch
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_decreased = true,
                 .lid_overdrive = true},
                // Now open back to the switch
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_increased = true,
                 .lid_overdrive = false},
                // Now overdrive into the switch
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_increased = true,
                 .lid_overdrive = true},
                // Should send ACK now
                {.msg = messages::LidStepperComplete(),
                 .ack =
                     messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::NO_ERROR}},
            };
            test_motor_state_machine(tasks, steps);
        }
    }
    GIVEN("lid is unknown at startup") {
        motor_policy.set_lid_closed_switch(false);
        motor_policy.set_lid_open_switch(false);
        WHEN("sending open lid command") {
            std::vector<MotorStep> steps = {
                // First step retracts seal switch
                {.msg = messages::OpenLidMessage{.id = 123},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = true},
                // Second step extends seeal switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false},
                // Third step opens hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_increased = true,
                 .lid_overdrive = false},
                // Fourth step overdrives hinge
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_increased = true,
                 .lid_overdrive = true},
                // Should send ACK now
                {.msg = messages::LidStepperComplete(),
                 .ack =
                     messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::NO_ERROR}},
            };
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending close lid command") {
            std::vector<MotorStep> steps = {
                // First step retracts seal to switch
                {.msg = messages::CloseLidMessage{.id = 123},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = true},
                // Second step extends seal from switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false},
                // Third step closes hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_decreased = true,
                 .lid_overdrive = false},
                // Fourth step overdrives hinge
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_decreased = true,
                 .lid_overdrive = true},
                // Now extend seal to switch
                {.msg = messages::LidStepperComplete(),
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = true},
                // Retract seal from switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = false},
                // Should send ACK now
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .ack =
                     messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::NO_ERROR}},
            };
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending plate lift command") {
            std::vector<MotorStep> steps = {
                // Command should end with error
                {.msg = messages::PlateLiftMessage{.id = 123},
                 .ack = messages::AcknowledgePrevious{
                     .responding_to_id = 123,
                     .with_error = errors::ErrorCode::LID_CLOSED}}};
            test_motor_state_machine(tasks, steps);
        }
    }
}
