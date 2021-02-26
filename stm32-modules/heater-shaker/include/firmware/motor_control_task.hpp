/*
 * Interface for the firmware-specific parts of the motor control task
 */

#pragma once
#include "FreeRTOS.h"
#include "task.h"

namespace motor_control_task {
     // Function to call to start the task
     auto start() -> TaskHandle_t;
}
