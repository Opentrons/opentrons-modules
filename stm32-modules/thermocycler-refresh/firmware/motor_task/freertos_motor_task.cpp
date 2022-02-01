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
#include "thermocycler-refresh/motor_task.hpp"
#include "thermocycler-refresh/tasks.hpp"

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
static MotorPolicy _policy;

static void handle_lid_stepper() {
    static_cast<void>(_task.get_message_queue().try_send_from_isr(
        messages::MotorMessage(messages::LidStepperComplete{})));
}

static void handle_seal_interrupt() { _policy.seal_tick(); }

// Actual function that runs inside the task
void run(void *param) {
    static_cast<void>(param);

    motor_hardware_callbacks callbacks = {
        .lid_stepper_complete = handle_lid_stepper,
        .seal_stepper_tick = handle_seal_interrupt};
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
