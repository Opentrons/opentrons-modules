
#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_comms_task.hpp"
#include "firmware/freertos_system_task.hpp"
#include "firmware/freertos_ui_task.hpp"
#include "firmware/system_stm32g4xx.h"
#include "ot_utils/freertos/freertos_task.hpp"
#include "task.h"

using EntryPoint = std::function<void(tasks::FirmwareTasks::QueueAggregator *)>;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto host_task_entry = EntryPoint(host_comms_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto system_task_entry = EntryPoint(system_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task_entry = EntryPoint(ui_control_task::run);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto aggregator = tasks::FirmwareTasks::QueueAggregator();

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto host_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::HOST_STACK_SIZE, EntryPoint>(
        host_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto system_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::SYSTEM_STACK_SIZE, EntryPoint>(
        system_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::UI_STACK_SIZE, EntryPoint>(
        ui_task_entry);

auto main() -> int {
    HardwareInit();
    host_task.start(tasks::HOST_TASK_PRIORITY, "HostComms", &aggregator);
    system_task.start(tasks::SYSTEM_TASK_PRIORITY, "System", &aggregator);
    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator);

    vTaskStartScheduler();
    return 0;
}
