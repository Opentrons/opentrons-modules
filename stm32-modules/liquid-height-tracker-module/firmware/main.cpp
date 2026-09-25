#include <functional>
#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/i2c_hardware.h"
#include "firmware/system_stm32g4xx.h"
#include "liquid-height-tracker-module/fdc1004.hpp"
#include "ot_utils/freertos/freertos_task.hpp"
#include "firmware/freertos_fdc1004_task.hpp" // Integrated Task Header Include Path
#include "systemwide.h"
#include "task.h"

#pragma GCC diagnostic push
// NOLINTNEXTLINE(clang-diagnostic-unknown-warning-option)
#pragma GCC diagnostic ignored "-Wvolatile"
#include "stm32g4xx_hal.h"
#pragma GCC diagnostic pop

using EntryPoint = std::function<void(tasks::FirmwareTasks::QueueAggregator *)>;
using EntryPointUI = std::function<void(tasks::FirmwareTasks::QueueAggregator *,
                                        i2c::hardware::I2C *)>;

namespace tasks {
static auto ui_task_entry = EntryPointUI(ui_control_task::run);
static auto host_comms_entry = EntryPoint(host_comms_control_task::run);
static auto system_task_entry = EntryPoint(system_control_task::run);
}  // namespace tasks

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto host_comms_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::COMMS_STACK_SIZE, EntryPoint>(
        tasks::host_comms_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::UI_STACK_SIZE, EntryPointUI>(
        tasks::ui_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto system_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::SYSTEM_STACK_SIZE, EntryPoint>(
        tasks::system_task_entry);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto aggregator = tasks::FirmwareTasks::QueueAggregator();

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c2_comms = i2c::hardware::I2C();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c_handles = I2CHandlerStruct{};

auto main() -> int {
    HardwareInit();
    i2c_hardware_init(&i2c_handles);
    i2c2_comms.set_handle(i2c_handles.i2c2, I2C_BUS_2);

    // Start background system, host communications, and user interface tasks
    system_task.start(tasks::SYSTEM_TASK_PRIORITY, "System", &aggregator);
    host_comms_task.start(tasks::COMMS_TASK_PRIORITY, "Comms", &aggregator);
    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator, &i2c2_comms);

    // Formally spin up the non-blocking liquid tracking task thread.
    // This safely passes the initialized I2C communications handle reference.
    fdc1004::tasks::FDC1004_Task_Register(&i2c2_comms);

    // Start the FreeRTOS scheduler
    vTaskStartScheduler();
    return 0;
}
