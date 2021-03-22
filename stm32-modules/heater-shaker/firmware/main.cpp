#include "FreeRTOS.h"
#include "firmware/freertos_comms_task.hpp"
#include "firmware/freertos_heater_task.hpp"
#include "firmware/freertos_message_queue.hpp"
#include "firmware/freertos_motor_task.hpp"
#include "firmware/freertos_ui_task.hpp"
#include "heater-shaker/tasks.hpp"

auto main() -> int {
    auto ui = ui_control_task::start();
    auto heater = heater_control_task::start();
    auto motor = motor_control_task::start();
    auto comms = host_comms_control_task::start();
    tasks::Tasks<FreeRTOSMessageQueue> tasks = {
        .heater = heater.task,
        .comms = comms.task,
        .motor = motor.task,
        .ui = ui.task,
    };
    tasks.heater.provide_tasks(&tasks);
    tasks.comms.provide_tasks(&tasks);
    tasks.motor.provide_tasks(&tasks);
    tasks.ui.provide_tasks(&tasks);
    vTaskStartScheduler();
    return 0;
}
