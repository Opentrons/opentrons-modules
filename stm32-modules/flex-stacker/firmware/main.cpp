#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
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

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_driver_task_entry = EntryPoint(motor_driver_task::run);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto motor_task_entry = EntryPoint(motor_control_task::run);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task_entry = EntryPoint(ui_control_task::run);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto host_comms_entry = EntryPoint(host_comms_control_task::run);

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
static auto host_comms_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::COMMS_STACK_SIZE, EntryPoint>(
        host_comms_entry);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto ui_task =
    ot_utils::freertos_task::FreeRTOSTask<tasks::UI_STACK_SIZE, EntryPoint>(
        ui_task_entry);

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    switch (GPIO_Pin) {
        case MOTOR_DIAG0_PIN:
            static_cast<void>(aggregator.send_from_isr(
                messages::GPIOInterruptMessage{.pin = GPIO_Pin}));
            break;
        default:
            break;
    }
}

auto main() -> int {
    HardwareInit();

    driver_task.start(tasks::MOTOR_DRIVER_TASK_PRIORITY, "Motor Driver",
                      &aggregator);
    motor_task.start(tasks::MOTOR_TASK_PRIORITY, "Motor", &aggregator);

    host_comms_task.start(tasks::COMMS_TASK_PRIORITY, "Comms", &aggregator);

    ui_task.start(tasks::UI_TASK_PRIORITY, "UI", &aggregator);
    vTaskStartScheduler();
    return 0;
}
