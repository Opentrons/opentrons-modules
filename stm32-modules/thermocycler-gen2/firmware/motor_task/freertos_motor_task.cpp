/*
 * firmware-specific internals and hooks for motor control
 */
#include "firmware/freertos_motor_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/motor_hardware.h"
#include "firmware/motor_policy.hpp"
#include "task.h"
#include "thermocycler-gen2/motor_task.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace motor_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<motor_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _motor_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                 "Motor Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = motor_task::MotorTask(_motor_queue);

static constexpr uint32_t main_stack_size = 500;

// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<StackType_t, main_stack_size> stack;
// Internal FreeRTOS data structure for the task

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StaticTask_t main_data;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static TaskHandle_t _local_task;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static MotorPolicy _policy(false);

/**
 * @brief This function is called after the lid stepper has stepped the
 * requested number of steps.
 */
static void handle_lid_stepper() {
    static_cast<void>(_task.get_message_queue().try_send_from_isr(
        messages::MotorMessage(messages::LidStepperComplete{})));
}

/** @brief This function is called for every seal motor tick, at 1MHz.*/
static void handle_seal_interrupt() { _policy.seal_tick(); }

/**
 * @brief This function is called when the seal motor signals an error flag.
 * Based on the type of error that was raised (an actual error, or a stall
 * flag) a SealStepperComplete message will be sent to the motor_task and
 * the Seal motor interrupt will be disabled.
 *
 * @param[in] error The specific flag that was raised
 */
static void handle_seal_error(MotorError_t error) {
    using namespace messages;

    static_cast<void>(motor_hardware_stop_seal_movement());
    if (error == MotorError::MOTOR_ERROR) {
        static_cast<void>(_task.get_message_queue().try_send_from_isr(
            messages::MotorMessage(SealStepperComplete{
                .reason = SealStepperComplete::CompletionReason::ERROR})));
    } else {  // error == MotorError::MOTOR_STALL
        static_cast<void>(_task.get_message_queue().try_send_from_isr(
            messages::MotorMessage(SealStepperComplete{
                .reason = SealStepperComplete::CompletionReason::STALL})));
    }
}

/**
 * @brief Callback invoked when the seal motor triggers one of the limit
 * switches. As of PCB Rev2, there is a limit switch on each end of travel
 * but they share a single line. Therefore, the switch triggering this must
 * be disambiguated by the context of the current movement.
 *
 */
static void handle_seal_limit_switch() {
    using namespace messages;
    static_cast<void>(_task.get_message_queue().try_send_from_isr(
        MotorMessage(SealStepperComplete{
            .reason = SealStepperComplete::CompletionReason::LIMIT})));
}

// Actual function that runs inside the task
void run(void *param) {
    static_cast<void>(param);

    // Share the seal switch line if the board rev is 1 or 2
    auto shared_seal_switches = board_revision::BoardRevisionIface::get() <
                                board_revision::BoardRevision::BOARD_REV_3;

    _policy = MotorPolicy(shared_seal_switches);

    motor_hardware_callbacks callbacks = {
        .lid_stepper_complete = handle_lid_stepper,
        .seal_stepper_tick = handle_seal_interrupt,
        .seal_stepper_error = handle_seal_error,
        .seal_stepper_limit_switch = handle_seal_limit_switch};
    motor_hardware_setup(&callbacks);
    while (true) {
        _task.run_once(_policy);
    }
}

// Starter function that creates and spins off the task
auto start()
    -> tasks::Task<TaskHandle_t, motor_task::MotorTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "MotorControl", stack.size(), &_task,
                                     1, stack.data(), &main_data);
    _local_task = handle;
    _motor_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}

}  // namespace motor_control_task
