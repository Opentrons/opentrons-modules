/*
 * firmware-specific functions, data, and hooks for host comms control
 */
#include "firmware/freertos_comms_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "task.h"

namespace host_comms_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<host_comms_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _comms_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                 "Comms Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = host_comms_task::HostCommsTask(_comms_queue);

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
    auto *task_class = static_cast<decltype(_task) *>(param);
    static_cast<void>(task_class);
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Function that creates and spins up the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   host_comms_task::HostCommsTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "HostCommsControl", stack.size(),
                                     nullptr, 1, stack.data(), &data);
    _comms_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = _task};
}
}  // namespace host_comms_control_task
