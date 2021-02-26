#include "FreeRTOS.h"

#include "firmware/heater_control_task.hpp"
#include "firmware/host_comms_control_task.hpp"
#include "firmware/motor_control_task.hpp"
#include "firmware/ui_control_task.hpp"

auto main() -> int {
    ui_control_task::start();
    heater_control_task::start();
    motor_control_task::start();
    host_comms_control_task::start();
    vTaskStartScheduler();
    return 0;
}
