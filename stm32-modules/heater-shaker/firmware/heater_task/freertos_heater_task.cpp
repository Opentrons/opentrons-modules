/*
 * firmware-specific internals and hooks for heater control
 */

#include "firmware/freertos_heater_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater_policy.hpp"
#include "task.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "heater_hardware.h"
#pragma GCC diagnostic pop

namespace heater_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static void handle_conversion(const conversion_results *results);

static constexpr uint32_t _stack_size = 500;
// Stack as an array because there's no added overhead and why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _stack_size> _stack;

// internal data structure for freertos to store task state
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t _data;

static FreeRTOSMessageQueue<heater_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _heater_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                  "Heater Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = heater_task::HeaterTask(_heater_queue);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr uint32_t _hardware_stack_size = 128;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _hardware_stack_size> _hardware_stack;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t _hardware_data;

struct HeaterTasks {
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    heater_hardware hardware;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    heater_task::HeaterTask<FreeRTOSMessageQueue> heater_main_task;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    TaskHandle_t hardware_task_handle;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    HeaterPolicy policy;
    HeaterTasks(FreeRTOSMessageQueue<heater_task::Message> &queue,
                void (*conversion_handler)(const conversion_results *))
        : hardware{.conversions_complete = conversion_handler,
                   .hardware_internal = nullptr},
          heater_main_task(queue),
          hardware_task_handle(nullptr),
          policy(&hardware) {}
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
HeaterTasks _heater_tasks(_heater_queue, handle_conversion);

static void handle_conversion(const conversion_results *results) {
    if (results == nullptr) {
        return;
    }
    static_cast<void>(
        _heater_tasks.heater_main_task.get_message_queue().try_send_from_isr(
            messages::HeaterMessage(messages::TemperatureConversionComplete{
                .pad_a = results->pad_a_val,
                .pad_b = results->pad_b_val,
                .board = results->onboard_val})));
}

// Actual function that runs the task
void run(void *param) {
    auto *local_tasks = static_cast<HeaterTasks *>(param);
    if (!local_tasks->policy.try_reset_power_good()) {
        // throw error
        auto &queue = _task.get_message_queue();
        static_cast<void>(queue.try_send(messages::HandleNTCSetupError{}));
    }
    while (true) {
        local_tasks->heater_main_task.run_once(local_tasks->policy);
    }
}

/*
** The heater hardware task exists to kick off ADC conversions by calling
** begin_conversions() and, implicitly, to drive the timing of the heater
** control loop. THe main heater task reacts to the message sent by
** handle_conversion above containing readings; those readings are created
** by this task calling heater_hardware_begin_conversions; and thus, the
** conversions will happen at the rate of  this task. That's why it pokes
** into the heater task to figure out how frequently it should run.
**
*/
void run_hardware_task(void *param) {
    auto *local_tasks = static_cast<HeaterTasks *>(param);
    heater_hardware_setup(&local_tasks->hardware);
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(
            &last_wake_time,
            // NOLINTNEXTLINE(readability-static-accessed-through-instance)
            local_tasks->heater_main_task.CONTROL_PERIOD_TICKS);
        heater_hardware_begin_conversions(&local_tasks->hardware);
    }
}

// Starter that spins up the thread
auto start() -> tasks::Task<TaskHandle_t,
                            heater_task::HeaterTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "HeaterControl", _stack.size(),
                                     &_heater_tasks, 1, _stack.data(), &_data);
    _heater_queue.provide_handle(handle);
    auto *hardware_handle = xTaskCreateStatic(
        run_hardware_task, "HeaterHardware", _hardware_stack.size(),
        &_heater_tasks, 1, _hardware_stack.data(), &_hardware_data);
    _heater_tasks.hardware_task_handle = hardware_handle;
    return tasks::Task<TaskHandle_t, decltype(_heater_tasks.heater_main_task)>{
        .handle = handle, .task = &_heater_tasks.heater_main_task};
}
}  // namespace heater_control_task
