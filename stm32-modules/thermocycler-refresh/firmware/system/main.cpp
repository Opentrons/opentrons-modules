

#include "FreeRTOS.h"
#include "firmware/freertos_comms_task.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/freertos_system_task.hpp"
#include "firmware/freertos_thermal_plate_task.hpp"
#include "system_stm32g4xx.h"
#include "thermocycler-refresh/tasks.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "system_hardware.h"
#pragma GCC diagnostic pop

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
tasks::Tasks<FreeRTOSMessageQueue> tasks_aggregator;

auto main() -> int {
    HardwareInit();

    auto system = system_control_task::start();
    auto comms = host_comms_control_task::start();
    auto thermal_plate = thermal_plate_control_task::start();
    tasks_aggregator.initialize(comms.task, system.task, thermal_plate.task);
    vTaskStartScheduler();
    return 0;
}
