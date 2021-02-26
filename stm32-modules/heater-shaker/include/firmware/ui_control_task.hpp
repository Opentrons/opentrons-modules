/*
 * Interface for the firmware-specific parts of the UI task
 */
#pragma once

#include "FreeRTOS.h"
#include "task.h"

namespace ui_control_task {
     // Function that actually starts the task
     auto start() -> TaskHandle_t;
}
