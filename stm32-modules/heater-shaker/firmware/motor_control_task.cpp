/*
 * firmware-specific internals and hooks for motor control
 */
#include "firmware/motor_control_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "task.h"

namespace motor_control_task {
static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::array<StackType_t, stack_size> stack;
// Internal FreeRTOS data structure for the task

StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Actual function that runs inside the task
void run(void *param) {  // NOLINT(misc-unused-parameters)
    static constexpr uint32_t delay_ticks = 100;
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Starter function that creates and spins off the task
auto start() -> TaskHandle_t {
    return xTaskCreateStatic(run, "MotorControl", stack.size(), nullptr, 1,
                             stack.data(), &data);
}

}  // namespace motor_control_task
