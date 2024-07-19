/*
 * Interface for the firmware-specific parts of the motor control task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "task.h"

namespace motor_control_task {

// Actual function that runs in the task
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void;

}  // namespace motor_control_task
