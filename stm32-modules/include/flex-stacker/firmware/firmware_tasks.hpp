/**
 * @file firmware_tasks.hpp
 * @brief Expands on generic tasks.hpp to provide specific typedefs
 * for the firmware build
 */
#pragma once

#include "firmware/freertos_message_queue.hpp"
#include "flex-stacker/tasks.hpp"

namespace tasks {

using FirmwareTasks = Tasks<FreeRTOSMessageQueue>;

constexpr size_t HOST_STACK_SIZE = 2048;
constexpr uint8_t HOST_TASK_PRIORITY = 1;

constexpr size_t SYSTEM_STACK_SIZE = 256;
constexpr uint8_t SYSTEM_TASK_PRIORITY = 1;

constexpr size_t COMMS_STACK_SIZE = 256;
constexpr uint8_t COMMS_TASK_PRIORITY = 1;

constexpr size_t MOTOR_STACK_SIZE = 256;
constexpr uint8_t MOTOR_TASK_PRIORITY = 1;

constexpr size_t MOTOR_DRIVER_STACK_SIZE = 256;
constexpr uint8_t MOTOR_DRIVER_TASK_PRIORITY = 1;

constexpr size_t UI_STACK_SIZE = 256;
constexpr uint8_t UI_TASK_PRIORITY = 1;

};  // namespace tasks
