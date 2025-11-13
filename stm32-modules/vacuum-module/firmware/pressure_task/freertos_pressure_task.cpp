#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/i2c_comms.hpp"
#include "firmware/pressure_policy.hpp"
#include "task.h"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/pressure_task.hpp"

namespace pressure_control_task {
using namespace messages;
using namespace pressure_policy;

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
static tasks::FirmwareTasks::PressureQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Pressure Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = pressure_task::PressureTask(_queue, nullptr, nullptr);

static void run_hardware_task(void* param) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(
            &last_wake_time,
            // NOLINTNEXTLINE(readability-static-accessed-through-instance)
            pdMS_TO_TICKS(pressure_task::CONTROL_PERIOD_MS));
        static_cast<void>(_queue.try_send(messages::GetPressureMessage{0}));
    }
}

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator,
         i2c::hardware::I2C* i2c1_comms, i2c::hardware::I2C* i2c2_comms,
         i2c::hardware::I2C* i2c3_comms) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    // second task to drive get pressure periodically
    // auto *hardware_handle = xTaskCreateStatic(
    xTaskCreateStatic(run_hardware_task, "HeaterHardware",
                      _hardware_stack.size(), nullptr, 1,
                      _hardware_stack.data(), &_hardware_data);

    auto policy = PressurePolicy(i2c1_comms, i2c2_comms, i2c3_comms);
    while (true) {
        _top_task.run_once(policy);
    }
}

}  // namespace pressure_control_task
