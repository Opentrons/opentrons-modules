#pragma once

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/i2c_comms.hpp"
#include "task.h"

namespace ui_control_task {

// Actual function that runs in the task
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c_comms) -> void;
}  // namespace ui_control_task

namespace host_comms_control_task {
// Actual function that runs in the task
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void;
}  // namespace host_comms_control_task

namespace system_control_task {
// Actual function that runs in the task
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void;
}  // namespace system_control_task

namespace pressure_control_task {
// Actual function that runs in the task
auto run(tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c_comms) -> void;
}  // namespace pressure_control_task
