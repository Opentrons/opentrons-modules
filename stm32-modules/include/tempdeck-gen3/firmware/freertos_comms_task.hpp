/*
 * Interface for the firmware-specifc parts of the host comms task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "task.h"
#include "tempdeck-gen3/host_comms_task.hpp"

namespace host_comms_control_task {

// Actual function that runs in the task
auto run(void *) -> void;
}  // namespace host_comms_control_task
