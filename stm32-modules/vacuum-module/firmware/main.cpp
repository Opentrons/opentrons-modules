#include <functional>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/i2c_hardware.h"
#include "firmware/pressure_sensor_hardware.h"
#include "firmware/pump_hardware.h"
#include "firmware/system_stm32g4xx.h"
#include "firmware/vent_hardware.h"
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
using EntryPointPressure = std::function<void(
    tasks::FirmwareTasks::QueueAggregator *, i2c::hardware::I2C *,
    i2c::hardware::I2C *, i2c::hardware::I2C *)>;
using EntryPointPump =
    std::function<void(tasks::FirmwareTasks::QueueAggregator *)>;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task_entry = EntryPointUI(ui_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto host_comms_entry = EntryPoint(host_comms_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto system_task_entry = EntryPoint(system_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto pressure_task_entry =
    EntryPointPressure(pressure_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto pump_task_entry = EntryPointPump(pump_control_task::run);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto host_comms_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::COMMS_STACK_SIZE, EntryPoint>(
        host_comms_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::UI_STACK_SIZE, EntryPointUI>(
        ui_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto system_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::SYSTEM_STACK_SIZE, EntryPoint>(
        system_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto pressure_task = ot_utils::freertos_task::FreeRTOSTask<
    tasks::PRESSURE_STACK_SIZE, EntryPointPressure>(pressure_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto pump_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::PUMP_STACK_SIZE,
                                          EntryPointPump>(pump_task_entry);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto aggregator = tasks::FirmwareTasks::QueueAggregator();

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c1_comms = i2c::hardware::I2C();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c2_comms = i2c::hardware::I2C();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c3_comms = i2c::hardware::I2C();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c_handles = I2CHandlerStruct{};

auto main() -> int {
    HardwareInit();

    vent_hardware_init();
    pump_hardware_init();
    pressure_sensor_hardware_init();
    i2c_hardware_init(&i2c_handles);

    i2c1_comms.set_handle(i2c_handles.i2c1, I2C_BUS_1);
    i2c2_comms.set_handle(i2c_handles.i2c2, I2C_BUS_2);
    i2c3_comms.set_handle(i2c_handles.i2c3, I2C_BUS_3);

    system_task.start(tasks::SYSTEM_TASK_PRIORITY, "System", &aggregator);
    host_comms_task.start(tasks::COMMS_TASK_PRIORITY, "Comms", &aggregator);
    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator, &i2c2_comms);
    pump_task.start(tasks::PUMP_TASK_PRIORITY, "Pump", &aggregator);
    pressure_task.start(tasks::PRESSURE_TASK_PRIORITY, "Pressure", &aggregator,
                        &i2c1_comms, &i2c2_comms, &i2c3_comms);

    vTaskStartScheduler();
    return 0;
}
