/*
 * firmware-specific functions, data, and hooks for host comms control
 */
#include "firmware/host_comms_control_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "task.h"

namespace host_comms_control_task {
static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Actual function that runs in the task
void run(void *param) {  // NOLINT(misc-unused-parameters)
    static constexpr uint32_t delay_ticks = 100;
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Function that creates and spins up the task
auto start() -> TaskHandle_t {
    return xTaskCreateStatic(run, "HostCommsControl", stack.size(), nullptr, 1,
                             stack.data(), &data);
}
}  // namespace host_comms_control_task
