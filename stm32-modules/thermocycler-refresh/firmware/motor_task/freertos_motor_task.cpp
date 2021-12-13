/*
 * firmware-specific internals and hooks for motor control
 */
#include "firmware/freertos_motor_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "task.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wregister"
extern "C" {
#include "stm32g4xx_hal.h"
}
#pragma GCC diagnostic pop

#include "firmware/freertos_message_queue.hpp"
#include "thermocycler-refresh/motor_task.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "motor_hardware.h"
#include "motor_policy.hpp"

namespace motor_control_task {

struct MotorTaskFreeRTOS {
    TaskHandle_t main_task;
    //TaskHandle_t control_task;
    motor_hardware_handles handles;
};

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
//static constexpr uint32_t mc_stack_size = 128;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
//std::array<StackType_t, mc_stack_size> control_task_stack;
// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<StackType_t, main_stack_size> stack;
// Internal FreeRTOS data structure for the task

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StaticTask_t main_data;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
//StaticTask_t control_task_data;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static MotorTaskFreeRTOS _local_task;

static void handle_lid_stepper() {
    static_cast<void>(_task.get_message_queue().try_send_from_isr(
        messages::MotorMessage(messages::LidStepperComplete{})));
}

// Actual function that runs inside the task
void run(void *param) {
    static_cast<void>(param);
    memset(&_local_task.handles, 0, sizeof(_local_task.handles));
    _local_task.handles.lid_stepper_complete = handle_lid_stepper;
    motor_hardware_setup(&_local_task.handles);
    auto policy = MotorPolicy(&_local_task.handles);

    while (true) {
        _task.run_once(policy);
    }
}

/*void run_control_task(void *param) {
    static_cast<void>(param);
    while (true) {
        vTaskDelay(1);
        uint16_t code = MC_RunMotorControlTasks();
        if (code != 0) {
            auto &queue = _task.get_message_queue();
            static_cast<void>(queue.try_send(messages::MotorMessage(
                messages::MotorSystemErrorMessage{.errors = code})));
        }
    }
}*/

// Starter function that creates and spins off the task
auto start()
    -> tasks::Task<TaskHandle_t, motor_task::MotorTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "MotorControl", stack.size(), &_task,
                                     1, stack.data(), &main_data);
    /*auto *control_task_handle = xTaskCreateStatic(
        run_control_task, "MCControl", control_task_stack.size(), nullptr, 2,
        control_task_stack.data(), &control_task_data);*/
    //_local_task.control_task = control_task_handle;
    _local_task.main_task = handle;
    _motor_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}

}  // namespace motor_control_task
