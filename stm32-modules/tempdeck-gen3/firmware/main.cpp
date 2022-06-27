
#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_comms_task.hpp"
#include "firmware/system_stm32g4xx.h"
#include "ot_utils/freertos/freertos_task.hpp"
#include "task.h"

using EntryPoint = std::function<void(tasks::FirmwareTasks::QueueAggregator *)>;

static auto host_task_entry = EntryPoint(host_comms_control_task::run);

static auto _aggregator = tasks::FirmwareTasks::QueueAggregator();
static auto host_task =
    ot_utils::freertos_task::FreeRTOSTask<2048, EntryPoint>(host_task_entry);

auto main() -> int {
    HardwareInit();
    host_task.start(1, "HostComms", &_aggregator);

    vTaskStartScheduler();
    return 0;
}
