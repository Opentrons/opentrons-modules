/*
 * Interface for the firmware-specific parts of the heater control task
 */
#pragma once

#include "FreeRTOS.h"
#include "task.h"

namespace heater_control_task {
     // Function that starts the task
     auto start() -> TaskHandle_t;
}
