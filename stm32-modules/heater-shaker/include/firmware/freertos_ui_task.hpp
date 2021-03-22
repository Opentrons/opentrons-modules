/*
 * Interface for the firmware-specific parts of the UI task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater-shaker/ui_task.hpp"
#include "task.h"

namespace ui_control_task {
// Function that actually starts the task
auto start()
    -> tasks::Task<TaskHandle_t, ui_task::UITask<FreeRTOSMessageQueue>>;
}  // namespace ui_control_task
