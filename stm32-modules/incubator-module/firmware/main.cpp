#include <functional>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_fdc1004_task.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/i2c_hardware.h"
#include "firmware/proximity_sensor.h"
#include "firmware/serial_hardware.h"
#include "firmware/system_stm32g4xx.h"
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
using EntryPointFDC1004 = std::function<void(
    tasks::FirmwareTasks::QueueAggregator *, i2c::hardware::I2C *)>;
using EntryPointNoArgs = std::function<void()>;

namespace tasks {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_driver_task_entry = EntryPoint(motor_driver_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_task_entry = EntryPoint(motor_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task_entry = EntryPointUI(ui_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto host_comms_entry = EntryPoint(host_comms_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto system_task_entry = EntryPoint(system_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto fdc1004_task_entry = EntryPointFDC1004(fdc1004::tasks::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto proximity_task_entry = EntryPoint(proximity_control_task::run);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto main_task_entry = EntryPointNoArgs(main_control_task::run);
}  // namespace tasks

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto driver_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::MOTOR_DRIVER_STACK_SIZE, EntryPoint>(
        tasks::motor_driver_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::MOTOR_STACK_SIZE, EntryPoint>(
        tasks::motor_task_entry);

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
static auto fdc1004_task = ot_utils::freertos_task::FreeRTOSTask<
    tasks::FDC1004_STACK_SIZE, EntryPointFDC1004>(tasks::fdc1004_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto proximity_task = ot_utils::freertos_task::FreeRTOSTask<
    tasks::PROXIMITY_STACK_SIZE, EntryPoint>(tasks::proximity_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto main_task = ot_utils::freertos_task::FreeRTOSTask<
    tasks::MAIN_STACK_SIZE, EntryPointNoArgs>(tasks::main_task_entry);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto aggregator = tasks::FirmwareTasks::QueueAggregator();

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c1_comms = i2c::hardware::I2C();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c_handles = I2CHandlerStruct{};

auto main() -> int {
    HardwareInit();

    if (serial_hardware_init()) {
        static constexpr char serial_startup_message[] =
            "USART2 serial output ready\r\n";
        static_cast<void>(serial_hardware_write(
            serial_startup_message, sizeof(serial_startup_message) - 1));
    }

    i2c_hardware_init(&i2c_handles);
    i2c1_comms.set_handle(i2c_handles.i2c1, I2C_BUS_1);
    proximity_sensor_init();

    // Start background system, host communications, and user interface tasks
    system_task.start(tasks::SYSTEM_TASK_PRIORITY, "System", &aggregator);
    driver_task.start(tasks::MOTOR_DRIVER_TASK_PRIORITY, "Motor Driver",
                            &aggregator);
    motor_task.start(tasks::MOTOR_TASK_PRIORITY, "Motor", &aggregator);
    host_comms_task.start(tasks::COMMS_TASK_PRIORITY, "Comms", &aggregator);
    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator, &i2c1_comms);
    // fdc1004_task.start(tasks::FDC1004_TASK_PRIORITY, "FDC1004", &aggregator,
    //                    &i2c1_comms);
    proximity_task.start(tasks::PROXIMITY_TASK_PRIORITY, "proximity_task",
                         &aggregator);
    main_task.start(tasks::MAIN_TASK_PRIORITY, "main_task");

    // Start the FreeRTOS scheduler
    vTaskStartScheduler();
    return 0;
}
