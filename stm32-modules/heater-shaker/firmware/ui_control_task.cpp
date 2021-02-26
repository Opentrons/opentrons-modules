/*
 * firmware-specific functions and data for ui control task
 */
#include "firmware/ui_control_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "task.h"

namespace ui_control_task {
static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure for the task
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Actual function that runs inside the task, unused param because we don't get
// to pick the function type
static void run(void *param) {  // NOLINT(misc-unused-parameters)
    static constexpr uint32_t delay_ticks = 100;
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Function that spins up the task
auto start() -> TaskHandle_t {
    return xTaskCreateStatic(run, "UIControl", stack.size(), nullptr, 1,
                             stack.data(), &data);
}
}  // namespace ui_control_task
