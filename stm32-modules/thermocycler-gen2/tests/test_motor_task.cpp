#include <iostream>
#include <utility>
#include "catch2/catch.hpp"
#include "systemwide.h"
#include "test/task_builder.hpp"
#include "thermocycler-gen2/messages.hpp"
#include "thermocycler-gen2/motor_task.hpp"
#include "thermocycler-gen2/motor_utils.hpp"


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

    using MotorState = messages::UpdateMotorState::MotorState;
    // If this isn't nullopt, check that the system task got a message
    // with this MotorState value
    std::optional<MotorState> motor_state = std::nullopt;

    using SealPos = motor_util::SealStepper::Status;
    // If this variable is set, expect seal in a specific position
    std::optional<SealPos> seal_pos = std::nullopt;
    // If this variable is set, check the lid velocity
    std::optional<double> lid_rpm = std::nullopt;
    // If true, expect an ack in the host comms task
    std::optional<messages::AcknowledgePrevious> ack = std::nullopt;
    bool lid_open_switch_engaged = false;
    bool lid_closed_switch_engaged = false;
    std::optional<double> test_id = std::nullopt;
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

        motor_policy.set_lid_open_switch(false);
        motor_policy.set_lid_closed_switch(false);
        auto lid_angle_before = motor_policy.get_angle();
        motor_queue.backing_deque.push_back(step.msg);
        tasks->get_system_queue().backing_deque.clear();

        if (step.lid_open_switch_engaged) {
            motor_policy.set_lid_open_switch(true);
        }
        if (step.lid_closed_switch_engaged) {
            motor_policy.set_lid_closed_switch(true);

        }


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
                    if(!tasks->get_host_comms_queue().has_message()) {
                    }
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
            if (step.motor_state.has_value()) {
                THEN("the motor state is updated correctly") {
                    REQUIRE(tasks->get_system_queue().has_message());
                    auto state_msg =
                        tasks->get_system_queue().backing_deque.front();
                    if (!std::holds_alternative<messages::UpdateMotorState>(
                            state_msg)) {
                    }
                    if (!std::holds_alternative<messages::UpdateMotorState>(
                            state_msg)) {
                    }
                    REQUIRE(std::holds_alternative<messages::UpdateMotorState>(
                        state_msg));
                    auto state =
                        std::get<messages::UpdateMotorState>(state_msg);
                    REQUIRE(state.state == step.motor_state);
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
                 .seal_switch_armed = true,
                 .motor_state = MotorStep::MotorState::OPENING_OR_CLOSING,
                .test_id = 1},
                // Second step extends seal switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false,
                 .test_id = 2},
                // Third step opens hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_increased = true,
                 .lid_overdrive = false,
                 .lid_rpm =
                     motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                .lid_open_switch_engaged = true,
                 .test_id = 3
                },
                // Fourth step overdrives hinge
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_decreased = true,
                 .lid_overdrive = true,
                 .lid_rpm =
                     motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                 .lid_open_switch_engaged = true,
                 .test_id = 4
                },
                // Should send ACK now
                {.msg = messages::LidStepperComplete(),
                 .motor_state = MotorStep::MotorState::IDLE,
                 .ack =
                     messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::NO_ERROR},
                 .test_id = 5},
            };
            test_motor_state_machine(tasks, steps);
        }
        GIVEN("seal lines aren't shared") {
            motor_policy.set_switch_lines_shared(false);
            WHEN("sending open lid command") {
                std::vector<MotorStep> steps = {
                    // First step retracts seal switch
                    {.msg = messages::OpenLidMessage{.id = 123},
                     .seal_on = true,
                     .seal_direction = true,
                     .seal_switch_armed = true,
                     .motor_state = MotorStep::MotorState::OPENING_OR_CLOSING,
                     .test_id = 6},
                    // Second step opens hinge
                    {.msg =
                         messages::SealStepperComplete{
                             .reason = messages::SealStepperComplete::
                                 CompletionReason::DONE},
                     .lid_angle_increased = true,
                     .lid_overdrive = false,
                     .lid_rpm =
                         motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                     .lid_open_switch_engaged = true,
                     .test_id = 7},
                    // Fourth step overdrives hinge
                    {.msg = messages::LidStepperComplete(),
                     .lid_angle_decreased = true,
                     .lid_overdrive = true,
                     .lid_open_switch_engaged = true,
                     .test_id = 77},
                    // Should send ACK now
                    {.msg = messages::LidStepperComplete(),
                     .motor_state = MotorStep::MotorState::IDLE,
                     .ack =
                         messages::AcknowledgePrevious{
                             .responding_to_id = 123,
                             .with_error = errors::ErrorCode::NO_ERROR},
                     .test_id = 8},
                };
                test_motor_state_machine(tasks, steps);
            }
        }
        WHEN("sending close lid command") {
            std::vector<MotorStep> steps = {
                // Command should end immediately
                {.msg = messages::CloseLidMessage{.id = 123},
                 .ack = messages::AcknowledgePrevious{
                     .responding_to_id = 123,
                     .with_error = errors::ErrorCode::NO_ERROR},
                 .lid_closed_switch_engaged = true,
                 .test_id = 9}};
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending plate lift command") {
            std::vector<MotorStep> steps = {
                // Command should end with error
                {.msg = messages::PlateLiftMessage{.id = 123},
                 .ack = messages::AcknowledgePrevious{
                     .responding_to_id = 123,
                     .with_error = errors::ErrorCode::LID_CLOSED},
                .test_id = 10
                }
                };
            test_motor_state_machine(tasks, steps);
        }
        GIVEN("seal retraction switch is triggered") {
            motor_policy.set_retraction_switch_triggered(true);
            WHEN("sending open lid command") {
                std::vector<MotorStep> steps = {
                    // First step retracts seal switch
                    {.msg = messages::OpenLidMessage{.id = 123},
                     .lid_angle_increased = true,
                     .lid_overdrive = false,
                     .lid_rpm =
                         motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                     .test_id = 11},
                };
                test_motor_state_machine(tasks, steps);
            }
        }
        GIVEN("seal retraction and extension switches are triggered") {
            motor_policy.set_retraction_switch_triggered(true);
            motor_policy.set_extension_switch_triggered(true);
            WHEN("sending open lid command") {
                std::vector<MotorStep> steps = {
                    // Should throw an error
                    {.msg = messages::OpenLidMessage{.id = 123},
                     .seal_on = false,
                     .ack = messages::AcknowledgePrevious{
                         .responding_to_id = 123,
                         .with_error = errors::ErrorCode::SEAL_MOTOR_SWITCH},
                     .test_id = 12}};
                test_motor_state_machine(tasks, steps);
            }
        }
        GIVEN("seal extension switch is triggered") {
            motor_policy.set_extension_switch_triggered(true);
            motor_policy.set_retraction_switch_triggered(false);
            WHEN("sending open lid command") {
                std::vector<MotorStep> steps = {
                    // Starts by retracting the seal
                    {.msg = messages::OpenLidMessage{.id = 123},
                     .seal_on = true,
                     .seal_direction = true,
                     .seal_switch_armed = true,
                     .motor_state = MotorStep::MotorState::OPENING_OR_CLOSING,
                     .test_id = 13}};
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
                         .with_error = errors::ErrorCode::NO_ERROR},
                 .lid_open_switch_engaged = true,
                 .test_id = 14},
            };
            test_motor_state_machine(tasks, steps);
        }
        WHEN("sending close lid command") {
            std::vector<MotorStep> steps = {
                // First step retracts seal to switch
                {.msg = messages::CloseLidMessage{.id = 123},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = true,
                 .motor_state = MotorStep::MotorState::OPENING_OR_CLOSING,
                 .test_id = 15},
                // Second step extends seal from switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false,
                 .test_id = 16},
                // Third step closes hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_decreased = true,
                 .lid_overdrive = false,
                 .lid_rpm =
                     motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                 .test_id = 17},
                // Fourth step overdrives hinge
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_decreased = true,
                 .lid_overdrive = true,
                 .lid_closed_switch_engaged = true,
                 .test_id = 189},
                // Now extend seal to switch
                {.msg = messages::LidStepperComplete(),
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = true,
                 .lid_closed_switch_engaged = true,
                 .test_id = 18},
                // Retract seal from switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = true,
                 .seal_switch_armed = false,
                 .test_id = 19},
            };
            AND_WHEN("the closed switch is triggered") {
                motor_policy.set_lid_closed_switch(true);
                steps.push_back(
                    // an ack with error code NO_ERROR should follow
                    MotorStep{.msg =
                                  messages::SealStepperComplete{
                                      .reason = messages::SealStepperComplete::
                                          CompletionReason::DONE},
                              .motor_state = MotorStep::MotorState::IDLE,
                              .ack = messages::AcknowledgePrevious{
                                  .responding_to_id = 123,
                                  .with_error = errors::ErrorCode::NO_ERROR},
                              .lid_closed_switch_engaged = true,
                              .test_id = 20});
                test_motor_state_machine(tasks, steps);
            }
            AND_WHEN("the closed switch is not triggered") {
                motor_policy.set_lid_closed_switch(false);
//                 the seal stepper should no longer move after a lid move failed
//                 edit this to test that an error message is received after a lidsteppercomplete
//                 with unexpected lid state
                steps.push_back(
//                     an ack with error code UNEXPECTED_LID_STATE should follow
                    MotorStep{
                        .msg =
                            messages::SealStepperComplete{
                                .reason = messages::SealStepperComplete::
                                    CompletionReason::DONE},
                        .motor_state = MotorStep::MotorState::IDLE,
                        .ack = messages::AcknowledgePrevious{
                            .responding_to_id = 123,
                            .with_error = errors::ErrorCode::UNEXPECTED_LID_STATE},
                        .lid_closed_switch_engaged = false,
                        .test_id = 21});
                test_motor_state_machine(tasks, steps);
            }
        }
        WHEN("sending plate lift command") {
            std::vector<MotorStep> steps;
            for (auto angle = motor_task::LidStepperState::
                     PLATE_LIFT_NUDGE_START_DEGREES;
                 angle <
                 motor_task::LidStepperState::PLATE_LIFT_NUDGE_FINAL_DEGREES +
                     motor_task::LidStepperState::PLATE_LIFT_NUDGE_INCREMENT;
                 angle +=
                 motor_task::LidStepperState::PLATE_LIFT_NUDGE_INCREMENT) {
                // Incremental nudging
                steps.push_back(MotorStep{
                    .msg = messages::LidStepperComplete(),
                    .lid_angle_increased = true,
                    .lid_overdrive = true,
                    .lid_rpm =
                        motor_task::LidStepperState::PLATE_LIFT_VELOCITY_RPM,
                    .lid_open_switch_engaged = true,
                    .lid_closed_switch_engaged = false,
                    .test_id = 22,
                });
                steps.push_back(MotorStep{
                    .msg = messages::LidStepperComplete(),
                    .lid_angle_decreased = true,
                    .lid_overdrive = true,
                    .lid_rpm =
                        motor_task::LidStepperState::PLATE_LIFT_VELOCITY_RPM,
                    .test_id = 23
                });
            }
            // Final open - all the way to the limit
            steps.push_back(MotorStep{
                .msg = messages::LidStepperComplete(),
                .lid_angle_increased = true,
                .lid_overdrive = true,
                .lid_rpm =
                    motor_task::LidStepperState::PLATE_LIFT_VELOCITY_RPM,
                .lid_open_switch_engaged = true,
                .test_id = 24});
            // Return below switch
            steps.push_back(MotorStep{
                .msg = messages::LidStepperComplete(),
                .lid_angle_increased = false,
                .lid_angle_decreased = true,
                .lid_overdrive = true,
                .lid_rpm =
                    motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                .lid_open_switch_engaged = true,
                .test_id = 25});
            // Return to the switch
            steps.push_back(MotorStep{
                .msg = messages::LidStepperComplete(),
                .lid_angle_increased = true,
                .lid_overdrive = false,
                .lid_rpm =
                    motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                .lid_open_switch_engaged = true,
                .test_id = 26,
            });
            // Now BACK OUT of the switch
            steps.push_back(MotorStep{
                .msg = messages::LidStepperComplete(),
                .lid_angle_increased = false,
                .lid_angle_decreased = true,
                .lid_overdrive = true,
//                .motor_state = MotorStep::MotorState::PLATE_LIFT,
                .lid_rpm =
                    motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                .lid_open_switch_engaged = true,
                .test_id = 27
            });
            steps.push_back(
                // an ack with error code NO_ERROR should follow
                MotorStep{.msg = messages::LidStepperComplete(),
                          .motor_state = MotorStep::MotorState::IDLE,
                          .ack = messages::AcknowledgePrevious{
                              .responding_to_id = 123,
                              .with_error = errors::ErrorCode::NO_ERROR},
                          .lid_open_switch_engaged = false,
                          .test_id = 28
                });
            steps[0].msg = messages::PlateLiftMessage{.id = 123};
            steps[0].motor_state = MotorStep::MotorState::PLATE_LIFT;
            test_motor_state_machine(tasks, steps);
        }
        GIVEN("seal retraction switch is triggered") {
            motor_policy.set_retraction_switch_triggered(true);
            WHEN("sending close lid command") {
                std::vector<MotorStep> steps = {
                    // First step closes hinge - skip seal movements
                    {.msg = messages::CloseLidMessage{.id = 123},
                     .lid_angle_decreased = true,
                     .lid_overdrive = false,
                     .lid_rpm =
                         motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                     .lid_closed_switch_engaged = true,
                    .test_id = 29},
                    // Fourth step overdrives hinge
                    {.msg = messages::LidStepperComplete(),
                     .lid_angle_decreased = true,
                     .lid_overdrive = true,
                     .test_id = 30},
                    // Now extend seal to switch
                    {.msg = messages::LidStepperComplete(),
                     .seal_on = true,
                     .seal_direction = false,
                     .seal_switch_armed = true,
                     .test_id = 31},
                    // Retract seal from switch
                    {.msg =
                         messages::SealStepperComplete{
                             .reason = messages::SealStepperComplete::
                                 CompletionReason::LIMIT},
                     .seal_on = true,
                     .seal_direction = true,
                     .seal_switch_armed = false,
                     .test_id = 32},
                };
                AND_WHEN("the closed switch is triggered") {
                    motor_policy.set_lid_closed_switch(true);
                    steps.push_back(
                        // an ack with error code NO_ERROR should follow
                        MotorStep{
                            .msg =
                                messages::SealStepperComplete{
                                    .reason = messages::SealStepperComplete::
                                        CompletionReason::DONE},
                            .motor_state = MotorStep::MotorState::IDLE,
                            .ack = messages::AcknowledgePrevious{
                                .responding_to_id = 123,
                                .with_error = errors::ErrorCode::NO_ERROR},
                            .test_id = 33});
                    test_motor_state_machine(tasks, steps);
                }
                AND_WHEN("the closed switch is not triggered") {
                    motor_policy.set_lid_closed_switch(false);
                    steps.push_back(
                        // an ack with error code UNEXPECTED_LID_STATE should
                        // follow
                        MotorStep{
                            .msg =
                                messages::SealStepperComplete{
                                    .reason = messages::SealStepperComplete::
                                        CompletionReason::DONE},
                            .motor_state = MotorStep::MotorState::IDLE,
                            .ack = messages::AcknowledgePrevious{
                                .responding_to_id = 0,
                                .with_error =
                                    errors::ErrorCode::UNEXPECTED_LID_STATE}});
                    test_motor_state_machine(tasks, steps);
                }
            }
        }
        GIVEN("seal switches aren't shared") {
            motor_policy.set_switch_lines_shared(false);
            WHEN("sending close lid command") {
                // Skip the 'backoff' steps
                std::vector<MotorStep> steps = {
                    // First step retracts seal to switch
                    {.msg = messages::CloseLidMessage{.id = 123},
                     .seal_on = true,
                     .seal_direction = true,
                     .seal_switch_armed = true,
                     .motor_state = MotorStep::MotorState::OPENING_OR_CLOSING},
                    // Second step closes hinge
                    {.msg =
                         messages::SealStepperComplete{
                             .reason = messages::SealStepperComplete::
                                 CompletionReason::DONE},
                     .lid_angle_decreased = true,
                     .lid_overdrive = false,
                     .lid_rpm =
                         motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM},
                    // Fourth step overdrives hinge
                    {.msg = messages::LidStepperComplete(),
                     .lid_angle_decreased = true,
                     .lid_overdrive = true},
                    // Now extend seal to switch
                    {.msg = messages::LidStepperComplete(),
                     .seal_on = true,
                     .seal_direction = false,
                     .seal_switch_armed = true},
                };
                AND_WHEN("the closed switch is triggered") {
                    motor_policy.set_lid_closed_switch(true);
                    steps.push_back(
                        // an ack with error code NO_ERROR should follow
                        MotorStep{
                            .msg =
                                messages::SealStepperComplete{
                                    .reason = messages::SealStepperComplete::
                                        CompletionReason::DONE},
                            .motor_state = MotorStep::MotorState::IDLE,
                            .ack = messages::AcknowledgePrevious{
                                .responding_to_id = 123,
                                .with_error = errors::ErrorCode::NO_ERROR}});
                    test_motor_state_machine(tasks, steps);
                }
                AND_WHEN("the closed switch is not triggered") {
                    motor_policy.set_lid_closed_switch(false);
                    steps.push_back(
                        // an ack with error code UNEXPECTED_LID_STATE should
                        // follow
                        MotorStep{
                            .msg =
                                messages::SealStepperComplete{
                                    .reason = messages::SealStepperComplete::
                                        CompletionReason::DONE},
                            .motor_state = MotorStep::MotorState::IDLE,
                            .ack = messages::AcknowledgePrevious{
                                .responding_to_id = 0,
                                .with_error =
                                    errors::ErrorCode::UNEXPECTED_LID_STATE}});
                    test_motor_state_machine(tasks, steps);
                }
            }
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
                 .seal_switch_armed = true,
                 .lid_open_switch_engaged = true},
                // Second step extends seal switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false,
                 .lid_open_switch_engaged = true},
                // Third step opens hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_increased = true,
                 .lid_overdrive = false,
                 .lid_rpm =
                     motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                .lid_open_switch_engaged = true},
                // Fourth step overdrives hinge
                {.msg = messages::LidStepperComplete(),
                 .lid_angle_decreased = true,
                 .lid_overdrive = true,
                 .lid_open_switch_engaged = false},
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
                 .seal_switch_armed = true,
                 .lid_closed_switch_engaged = true,
                 .test_id = 1},
                // Second step extends seal from switch
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::LIMIT},
                 .seal_on = true,
                 .seal_direction = false,
                 .seal_switch_armed = false,
                 .lid_closed_switch_engaged = true,
                 .test_id = 2},
                // Third step closes hinge
                {.msg =
                     messages::SealStepperComplete{
                         .reason = messages::SealStepperComplete::
                             CompletionReason::DONE},
                 .lid_angle_decreased = true,
                 .lid_overdrive = false,
                 .lid_rpm =
                     motor_task::LidStepperState::LID_DEFAULT_VELOCITY_RPM,
                 .lid_closed_switch_engaged = true,
                 .test_id = 3}};
            test_motor_state_machine(tasks, steps);
            steps.clear();
            AND_WHEN("the closed switch is triggered") {
                motor_policy.set_lid_closed_switch(true);
                // Fourth step overdrives hinge
                steps.push_back(MotorStep{.msg = messages::LidStepperComplete(),
                                          .lid_angle_decreased = true,
                                          .lid_overdrive = true,
                                          .lid_closed_switch_engaged = true,
                                          .test_id = 4
                });
                // Now extend seal to switch
                steps.push_back(MotorStep{.msg = messages::LidStepperComplete(),
                                          .seal_on = true,
                                          .seal_direction = false,
                                          .seal_switch_armed = true,
                                          .lid_closed_switch_engaged = true,
                                          .test_id = 5
                });
                // Retract seal from switch
                steps.push_back(
                    MotorStep{.msg =
                                  messages::SealStepperComplete{
                                      .reason = messages::SealStepperComplete::
                                          CompletionReason::LIMIT},
                              .seal_on = true,
                              .seal_direction = true,
                              .seal_switch_armed = false,
                              .test_id = 5});
                steps.push_back(
                    // an ack with error code NO_ERROR should follow
                    MotorStep{.msg =
                                  messages::SealStepperComplete{
                                      .reason = messages::SealStepperComplete::
                                          CompletionReason::DONE},
                              .ack = messages::AcknowledgePrevious{
                                  .responding_to_id = 123,
                                  .with_error = errors::ErrorCode::NO_ERROR},
                              .test_id = 6});
                test_motor_state_machine(tasks, steps);
            }
            AND_WHEN("the closed switch is not triggered") {
                motor_policy.set_lid_closed_switch(false);
                // Fourth step overdrives hinge
                steps.push_back(MotorStep{.msg = messages::LidStepperComplete(),
                                          .lid_angle_decreased = true,
                                          .lid_overdrive = true,
                                          .test_id = 7});
                // Now extend seal to switch
                steps.push_back(MotorStep{.msg = messages::LidStepperComplete(),
                                          .seal_on = true,
                                          .seal_direction = false,
                                          .seal_switch_armed = true});
                // Retract seal from switch
                steps.push_back(
                    MotorStep{.msg =
                                  messages::SealStepperComplete{
                                      .reason = messages::SealStepperComplete::
                                          CompletionReason::LIMIT},
                              .seal_on = true,
                              .seal_direction = true,
                              .seal_switch_armed = false});
                steps.push_back(
//                    // an ack with error code UNEXPECTED_LID_STATE should follow
                    MotorStep{
                        .msg =
                            messages::SealStepperComplete{
                                .reason = messages::SealStepperComplete::
                                    CompletionReason::DONE},
                        .ack = messages::AcknowledgePrevious{
                            .responding_to_id = 0,
                            .with_error =
                                errors::ErrorCode::UNEXPECTED_LID_STATE}
                    });
                test_motor_state_machine(tasks, steps);
            }
        }
        WHEN("sending plate lift command") {
            motor_policy.set_lid_closed_switch(true);
            std::vector<MotorStep> steps = {
                // Command should end with error
                {.msg = messages::PlateLiftMessage{.id = 123},
                 .ack = messages::AcknowledgePrevious{
                     .responding_to_id = 123,
                     .with_error = errors::ErrorCode::LID_CLOSED},
                .lid_open_switch_engaged = false,
                .lid_closed_switch_engaged = true}};
            test_motor_state_machine(tasks, steps);
        }
    }
}
