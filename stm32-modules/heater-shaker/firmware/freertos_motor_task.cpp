/*
 * firmware-specific internals and hooks for motor control
 */
#include "firmware/freertos_motor_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/motor_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "task.h"

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

static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<StackType_t, stack_size> stack;
// Internal FreeRTOS data structure for the task

StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Actual function that runs inside the task
void run(void *param) {
    auto *task_class = static_cast<decltype(_task) *>(param);
    while (true) {
        task_class->run_once();
    }
}

// Starter function that creates and spins off the task
auto start()
    -> tasks::Task<TaskHandle_t, motor_task::MotorTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "MotorControl", stack.size(), &_task,
                                     1, stack.data(), &data);

    _motor_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}

}  // namespace motor_control_task
