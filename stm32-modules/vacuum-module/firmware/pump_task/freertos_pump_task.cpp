#include <array>
#include <atomic>
#include <cstdint>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/pump_policy.hpp"
#include "portmacro.h"
#include "task.h"
#include "vacuum-module/pump_task.hpp"

namespace pump_control_task {
using namespace messages;
using namespace pump_policy;

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr uint32_t _hardware_stack_size = 128;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _hardware_stack_size> _hardware_stack;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t _hardware_data;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> t_resync_needed = true;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::PumpQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE), "Pump Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = pump_task::PumpTask(_queue, nullptr, nullptr);

static void run_hardware_task(void* param) {
    static_cast<void>(param);
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        // Need to resync last time count after suspension
        if (t_resync_needed.load()) {
            last_wake_time = xTaskGetTickCount();
            t_resync_needed.store(false);
        }

        vTaskDelayUntil(
            &last_wake_time,
            // NOLINTNEXTLINE(readability-static-accessed-through-instance)
            pdMS_TO_TICKS(pump_task::CONTROL_PERIOD_MS));
        static_cast<void>(_queue.try_send(PumpControlMessage{}));
    }
}

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    // second task to drive pump motor
    auto* hw_handle = xTaskCreateStatic(
        run_hardware_task, "PumpHardware", _hardware_stack.size(), nullptr, 1,
        _hardware_stack.data(), &_hardware_data);

    auto policy = PumpPolicy(hw_handle, &t_resync_needed);
    policy.enable_pump_control(false);
    while (true) {
        _top_task.run_once(policy);
    }
}

}  // namespace pump_control_task
