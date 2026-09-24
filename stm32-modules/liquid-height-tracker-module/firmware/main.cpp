#include <functional>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/i2c_hardware.h"
#include "firmware/system_stm32g4xx.h"
#include "liquid-height-tracker-module/fdc1004.hpp"
#include "ot_utils/freertos/freertos_task.hpp"
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

/**
 * @brief One-shot FreeRTOS task that exercises the FDC1004 driver over I2C.
 * This MUST run from within a task (after the scheduler has started),
 * because the I2C driver on this board blocks on FreeRTOS task
 * notifications to signal transfer completion.
 */
extern "C" void fdc1004_test_task(void *unused) {
    static_cast<void>(unused);
    auto sensor = fdc1004::FDC1004(&i2c2_comms);

    if (!sensor.who_am_i()) {
        vTaskSuspend(nullptr);
    }
    if (!sensor.reset()) {
        vTaskSuspend(nullptr);
    }
    if (!sensor.configure_single_ended(0, 10)) {
        vTaskSuspend(nullptr);
    }
    if (!sensor.trigger_measurement(0, fdc1004::FDC1004::Rate::SAMPLES_100HZ)) {
        vTaskSuspend(nullptr);
    }
    while (!sensor.is_measurement_done(0)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    double capacitance_pf = 0.0;
    if (!sensor.read_measurement(0, capacitance_pf)) {
        vTaskSuspend(nullptr);
    }
    vTaskSuspend(nullptr);
}

auto main() -> int {
    HardwareInit();
    i2c_hardware_init(&i2c_handles);
    i2c2_comms.set_handle(i2c_handles.i2c2, I2C_BUS_2);

    system_task.start(tasks::SYSTEM_TASK_PRIORITY, "System", &aggregator);
    host_comms_task.start(tasks::COMMS_TASK_PRIORITY, "Comms", &aggregator);
    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator, &i2c2_comms);

    xTaskCreate(fdc1004_test_task, "FDC1004Test", 512, nullptr,
                tasks::SYSTEM_TASK_PRIORITY, nullptr);

    vTaskStartScheduler();
    return 0;
}
