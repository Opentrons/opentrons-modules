/*
 * firmware-specific internals and hooks for heater control
 */
#include "firmware/heater_control_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "task.h"

namespace heater_control_task {

static constexpr uint32_t stack_size = 500;
// Stack as an array because there's no added overhead and why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// internal data structure for freertos to store task state
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Actual function that runs the task
void run(void *param) {  // NOLINT(misc-unused-parameters)
    static constexpr uint32_t delay_ticks = 100;
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Starter that spins up the thread
auto start() -> TaskHandle_t {
    return xTaskCreateStatic(run, "HeaterControl", stack.size(), nullptr, 1,
                             stack.data(), &data);
}
}  // namespace heater_control_task
