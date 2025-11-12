#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/pump_policy.hpp"
#include "vacuum-module/pump_task.hpp"
#include "task.h"

namespace pump_control_task {
using namespace pump_policy;
enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::PumpQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE), "Pump Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = pump_task::PumpTask(_queue, nullptr, nullptr);

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c1_comms) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    auto policy = PumpPolicy();
    while (true) {
        _top_task.run_once(policy);
    }
}

}  // namespace pump_control_task
