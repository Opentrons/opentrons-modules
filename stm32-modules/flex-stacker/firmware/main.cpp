#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/system_stm32g4xx.h"
#include "ot_utils/freertos/freertos_task.hpp"
#include "systemwide.h"
#include "task.h"

using EntryPoint = std::function<void(tasks::FirmwareTasks::QueueAggregator *)>;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_driver_task_entry = EntryPoint(motor_driver_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_task_entry = EntryPoint(motor_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task_entry = EntryPoint(ui_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto aggregator = tasks::FirmwareTasks::QueueAggregator();

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto driver_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::MOTOR_DRIVER_STACK_SIZE,
                                          EntryPoint>(motor_driver_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::MOTOR_STACK_SIZE, EntryPoint>(
        motor_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::UI_STACK_SIZE, EntryPoint>(
        ui_task_entry);

auto main() -> int {
    HardwareInit();

    driver_task.start(tasks::MOTOR_DRIVER_TASK_PRIORITY, "Motor Driver",
                      &aggregator);
    motor_task.start(tasks::MOTOR_TASK_PRIORITY, "Motor", &aggregator);

    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator);
    vTaskStartScheduler();
    return 0;
}
