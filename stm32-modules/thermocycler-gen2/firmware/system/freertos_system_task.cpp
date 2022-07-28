/*
 * firmware-specific functions and data for ui control task
 */
#include "firmware/freertos_system_task.hpp"

#include <array>
#include <atomic>

#include "FreeRTOS.h"
#include "core/timer.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/freertos_timer.hpp"
#include "firmware/system_hardware.h"
#include "firmware/system_led_hardware.h"
#include "firmware/system_policy.hpp"
#include "task.h"
#include "thermocycler-gen2/board_revision.hpp"
#include "thermocycler-gen2/system_task.hpp"
#include "thermocycler-gen2/tasks.hpp"

namespace system_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<system_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _system_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                  "UI Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = system_task::SystemTask(_system_queue);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto policy = SystemPolicy();

static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure for the task
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Periodic timer for UI updates
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static timer::GenericTimer<freertos_timer::FreeRTOSTimer> _led_timer(
    "led timer", decltype(_task)::LED_UPDATE_PERIOD_MS, true,
    [ObjectPtr = &_task] { ObjectPtr->led_timer_callback(); });

// Periodic timer for Front Button LED Updates
static timer::GenericTimer<freertos_timer::FreeRTOSTimer>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _front_button_led_timer("button led",
                            decltype(_task)::FRONT_BUTTON_PERIOD_MS, true,
                            [ObjectPtr = &_task] {
                                ObjectPtr->front_button_led_callback(policy);
                            });

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::atomic<TaskHandle_t> _button_task_handle = nullptr;

// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> button_stack;

// Internal FreeRTOS data structure for the button's task
static StaticTask_t
    button_data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * @brief This is the DIRECT callback from .c file that will unblock the
 * Button Task to handle a new button press
 *
 */
static auto front_button_callback() -> void {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (_button_task_handle != nullptr) {
        vTaskNotifyGiveFromISR(_button_task_handle, &xHigherPriorityTaskWoken);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void run_button_task(void *param) {
    static_cast<void>(param);
    _button_task_handle = xTaskGetCurrentTaskHandle();
    auto last_wake_time = xTaskGetTickCount();
    auto button = system_task::ButtonPress(
        [task_ptr = &_task](bool long_press) {
            task_ptr->front_button_callback(long_press);
        },
        decltype(_task)::LONG_PRESS_TIME_MS);

    while (true) {
        // First, wait until we get unlocked
        static_cast<void>(ulTaskNotifyTake(pdTRUE, portMAX_DELAY));
        button.reset();
        // Now, do an initial debounce
        last_wake_time = xTaskGetTickCount();
        auto button_press_start = last_wake_time;
        vTaskDelayUntil(&last_wake_time, FRONT_BUTTON_DEBOUNCE_MS);

        do {
            // Sleep 50ms and see if the button is released
            vTaskDelayUntil(&last_wake_time, FRONT_BUTTON_QUERY_RATE_MS);
            button.update_held(last_wake_time - button_press_start);
            button_press_start = xTaskGetTickCount();
        } while (system_front_button_pressed());

        button.released(0);

        // Now debounce on the button release so that we don't accidentally
        // register it as a press!
        vTaskDelayUntil(&last_wake_time, FRONT_BUTTON_DEBOUNCE_MS);

        // Finally, clear out the task notifications just in case the button
        // debouncing resulted in extra notifications
        static_cast<void>(ulTaskNotifyTake(pdTRUE, 0));
    }
}

// Actual function that runs inside the task, unused param because we don't get
// to pick the function type
static void run(void *param) {
    using namespace board_revision;
    system_hardware_setup(
        BoardRevisionIface::get() == BoardRevision::BOARD_REV_1,
        front_button_callback);
    system_led_initialize();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto *task = reinterpret_cast<decltype(_task) *>(param);

    _led_timer.start();
    _front_button_led_timer.start();
    while (true) {
        task->run_once(policy);
    }
}

// Function that spins up the task
auto start() -> tasks::Task<TaskHandle_t,
                            system_task::SystemTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "SystemControl", stack.size(), &_task,
                                     1, stack.data(), &data);
    _system_queue.provide_handle(handle);

    static_cast<void>(xTaskCreateStatic(run_button_task, "FrontButton",
                                        button_stack.size(), nullptr, 1,
                                        button_stack.data(), &button_data));
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}
}  // namespace system_control_task
