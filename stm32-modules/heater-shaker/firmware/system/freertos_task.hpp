/* freertos_task.hpp - template for the freertos implementation of tasks
 */

#pragma once

#include "FreeRTOS.h"
#include "task.h"

namespace freertos_task {
template <class Task>
struct FreeRTOSTask {
    TaskHandle_t handle;
    Task& task;
};
}  // namespace freertos_task
