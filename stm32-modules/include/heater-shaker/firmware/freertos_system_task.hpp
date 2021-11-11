/*
 * Interface for the firmware-specific parts of the UI task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/system_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "task.h"

namespace system_control_task {
// Function that actually starts the task
auto start()
    -> tasks::Task<TaskHandle_t, system_task::SystemTask<FreeRTOSMessageQueue>>;
}  // namespace system_control_task
