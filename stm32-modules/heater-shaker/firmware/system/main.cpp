

#include "FreeRTOS.h"
#include "firmware/freertos_comms_task.hpp"
#include "firmware/freertos_heater_task.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/freertos_motor_task.hpp"
#include "firmware/freertos_system_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "system_stm32f3xx.h"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
tasks::Tasks<FreeRTOSMessageQueue> tasks_aggregator;

auto main() -> int {
    HardwareInit();
    auto system = system_control_task::start();
    auto heater = heater_control_task::start();
    auto motor = motor_control_task::start();
    auto comms = host_comms_control_task::start();
    tasks_aggregator.initialize(heater.task, comms.task, motor.task,
                                system.task);
    vTaskStartScheduler();
    return 0;
}
