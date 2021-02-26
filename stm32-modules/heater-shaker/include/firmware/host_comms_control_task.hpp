/*
 * Interface for the firmware-specifc parts of the host comms task
 */
#pragma once
#include "FreeRTOS.h"
#include "task.h"

namespace host_comms_control_task {
     // Function that starts the task
     auto start() -> TaskHandle_t;
}
