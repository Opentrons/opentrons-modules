/*
 * firmware-specific functions and data for ui control task
 */
#include "firmware/freertos_system_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/system_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "system_policy.hpp"
#include "task.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#pragma GCC diagnostic pop

namespace system_control_task {

struct SystemTaskFreeRTOS {
    system_hardware_handles handles;
};

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<system_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _system_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                  "UI Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = system_task::SystemTask(_system_queue);

static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure for the task
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static SystemTaskFreeRTOS _local_task;

static void handle_led_transmit_callback(const led_transmit_result *result) {
    if (result == nullptr) {
        return;
    }
    static_cast<void>(_task.get_message_queue().try_send_from_isr(
        messages::SystemMessage(messages::LEDTransmitComplete{
            .transmitted = result->success, .error = false})));
}

static void handle_led_transmit_error_callback(const led_transmit_result *result) {
    if (result == nullptr) {
        return;
    }
    static_cast<void>(_task.get_message_queue().try_send_from_isr(
        messages::SystemMessage(messages::LEDTransmitComplete{
            .transmitted = result->success, .error = result->error})));
}

// Actual function that runs inside the task, unused param because we don't get
// to pick the function type
static void run(void *param) {
    memset(&_local_task.handles, 0, sizeof(_local_task.handles));
    _local_task.handles.led_transmit_complete = handle_led_transmit_callback;
    _local_task.handles.led_transmit_error_complete = handle_led_transmit_error_callback;
    system_hardware_setup(&_local_task.handles);
    static constexpr uint32_t delay_ticks = 100;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto *task = reinterpret_cast<decltype(_task) *>(param);
    auto policy = SystemPolicy();
    while (true) {
        task->run_once(policy);
        vTaskDelay(delay_ticks);
    }
}

// Function that spins up the task
auto start() -> tasks::Task<TaskHandle_t,
                            system_task::SystemTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "SystemControl", stack.size(), &_task,
                                     1, stack.data(), &data);
    _system_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}
}  // namespace system_control_task
