/*
 * firmware-specific internals and hooks for the lid-heater task
 */

#include "firmware/freertos_lid_heater_task.hpp"

#include "FreeRTOS.h"
#include "lid_heater_policy.hpp"
#include "thermocycler-refresh/lid_heater_task.hpp"

namespace lid_heater_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<lid_heater_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _lid_heater_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                      "Lid Heater Queue");

static auto _task = lid_heater_task::LidHeaterTask(_lid_heater_queue);

static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not. Quiet lint because, well, we have to
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure for the task
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void run(void *param) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto *task = reinterpret_cast<decltype(_task) *>(param);
    auto policy = LidHeaterPolicy();
    while (true) {
        task->run_once(policy);
    }
}

// Function that spins up the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   lid_heater_task::LidHeaterTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "LidHeater", stack.size(), &_task, 1,
                                     stack.data(), &data);
    _lid_heater_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}
}  // namespace lid_heater_control_task
