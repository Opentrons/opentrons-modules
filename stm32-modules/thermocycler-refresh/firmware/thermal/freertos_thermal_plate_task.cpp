/*
 * firmware-specific internals and hooks for the plate task
 */

#include "firmware/freertos_thermal_plate_task.hpp"

#include "FreeRTOS.h"
#include "thermal_plate_policy.hpp"
#include "thermocycler-refresh/thermal_plate_task.hpp"

namespace thermal_plate_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<thermal_plate_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _thermal_plate_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                         "Thermal Plate Queue");

static auto _task = thermal_plate_task::ThermalPlateTask(_thermal_plate_queue);

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
    auto policy = ThermalPlatePolicy();
    while (true) {
        task->run_once(policy);
    }
}

// Function that spins up the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   thermal_plate_task::ThermalPlateTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "ThermalPlate", stack.size(), &_task,
                                     1, stack.data(), &data);
    _thermal_plate_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = &_task};
}
}  // namespace thermal_plate_control_task
