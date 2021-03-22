/*
 * firmware-specific functions and data for ui control task
 */
#include "firmware/freertos_ui_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater-shaker/ui_task.hpp"
#include "task.h"

namespace ui_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<ui_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _ui_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
              "UI Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = ui_task::UITask(_ui_queue);

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
auto start()
    -> tasks::Task<TaskHandle_t, ui_task::UITask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "UIControl", stack.size(), &_task, 1,
                                     stack.data(), &data);
    _ui_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = _task};
}
}  // namespace ui_control_task
