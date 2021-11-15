/*
 * Interface for the firmware-specific parts of the thermal plate control task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "task.h"
#include "thermocycler-refresh/tasks.hpp"

namespace lid_heater_control_task {
// Function that starts the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   lid_heater_task::LidHeaterTask<FreeRTOSMessageQueue>>;
}  // namespace lid_heater_control_task
