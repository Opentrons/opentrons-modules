/*
 * Interface for the firmware-specifc parts of the thermal tasks
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "task.h"

namespace thermistor_control_task {

// Actual function that runs in the task
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void;
}  // namespace thermistor_control_task
