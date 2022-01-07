

#include "FreeRTOS.h"
#include "firmware/freertos_comms_task.hpp"
#include "firmware/freertos_lid_heater_task.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/freertos_system_task.hpp"
#include "firmware/freertos_thermal_plate_task.hpp"
#include "firmware/system_hardware.h"
#include "system_stm32g4xx.h"
#include "thermocycler-refresh/board_revision.hpp"
#include "thermocycler-refresh/tasks.hpp"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
tasks::Tasks<FreeRTOSMessageQueue> tasks_aggregator;

auto main() -> int {
    HardwareInit();
    // Read the board revision here to make sure it's cached for the rest
    // of program execution
    auto revision = board_revision::BoardRevisionIface::get();
    configASSERT(revision != board_revision::BoardRevision::BOARD_REV_INVALID);

    auto system = system_control_task::start();
    auto comms = host_comms_control_task::start();
    auto thermal_plate = thermal_plate_control_task::start();
    auto lid_heater = lid_heater_control_task::start();
    tasks_aggregator.initialize(comms.task, system.task, thermal_plate.task,
                                lid_heater.task);
    vTaskStartScheduler();
    return 0;
}
