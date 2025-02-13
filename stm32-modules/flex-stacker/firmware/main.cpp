#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/i2c_hardware.h"
#include "firmware/motor_hardware.h"
#include "firmware/system_stm32g4xx.h"
#include "flex-stacker/messages.hpp"
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
using EntryPointTOF = std::function<void(
    tasks::FirmwareTasks::QueueAggregator *, i2c::hardware::I2C *)>;

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
static auto tof_sensor_entry = EntryPointTOF(tof_sensor_task::run);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto driver_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::MOTOR_DRIVER_STACK_SIZE,
                                          EntryPoint>(motor_driver_task_entry);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::MOTOR_STACK_SIZE, EntryPoint>(
        motor_task_entry);
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
static auto tof_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::TOF_SENSOR_STACK_SIZE,
                                          EntryPointTOF>(tof_sensor_entry);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto aggregator = tasks::FirmwareTasks::QueueAggregator();

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c2_comms = i2c::hardware::I2C();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c3_comms = i2c::hardware::I2C();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto i2c_handles = I2CHandlerStruct{};

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    switch (GPIO_Pin) {
        case MOTOR_DIAG0_PIN:
        case N_ESTOP_PIN:
            static_cast<void>(aggregator.send_from_isr(
                messages::GPIOInterruptMessage{.pin = GPIO_Pin}));
            break;
        default:
            break;
    }
}

auto main() -> int {
    HardwareInit();

    i2c_hardware_init(&i2c_handles);

    i2c2_comms.set_handle(i2c_handles.i2c2, I2C_BUS_2);
    i2c3_comms.set_handle(i2c_handles.i2c3, I2C_BUS_3);

    system_task.start(tasks::SYSTEM_TASK_PRIORITY, "System", &aggregator);
    driver_task.start(tasks::MOTOR_DRIVER_TASK_PRIORITY, "Motor Driver",
                      &aggregator);
    tof_task.start(tasks::TOF_SENSOR_TASK_PRIORITY, "TOF Sensor", &aggregator,
                   &i2c3_comms);
    motor_task.start(tasks::MOTOR_TASK_PRIORITY, "Motor", &aggregator);
    host_comms_task.start(tasks::COMMS_TASK_PRIORITY, "Comms", &aggregator);
    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator, &i2c2_comms);

    vTaskStartScheduler();
    return 0;
}
