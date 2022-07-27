/*
 * Interface for the firmware-specifc parts of the system task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "task.h"

namespace ui_control_task {

// Actual function that runs in the task
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void;
}  // namespace ui_control_task
