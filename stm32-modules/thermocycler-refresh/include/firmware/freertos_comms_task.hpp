/*
 * Interface for the firmware-specifc parts of the host comms task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "thermocycler-refresh/host_comms_task.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "task.h"

namespace host_comms_control_task {
// Function that starts the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   host_comms_task::HostCommsTask<FreeRTOSMessageQueue>>;
}  // namespace host_comms_control_task
