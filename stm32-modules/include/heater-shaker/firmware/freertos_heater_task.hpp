/*
 * Interface for the firmware-specific parts of the heater control task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "task.h"

namespace heater_control_task {
// Function that starts the task
auto start()
    -> tasks::Task<TaskHandle_t, heater_task::HeaterTask<FreeRTOSMessageQueue>>;
}  // namespace heater_control_task
