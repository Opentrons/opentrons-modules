/*
 * Interface for the firmware-specific parts of the motor control task
 */

#pragma once
#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "task.h"
#include "thermocycler-refresh/motor_task.hpp"
#include "thermocycler-refresh/tasks.hpp"

namespace motor_control_task {

// Function to call to start the task
auto start()
    -> tasks::Task<TaskHandle_t, motor_task::MotorTask<FreeRTOSMessageQueue>>;
}  // namespace motor_control_task
