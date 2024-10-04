#include "FreeRTOS.h"
#include "climits"
#include "firmware/freertos_tasks.hpp"
#include "firmware/motor_driver_policy.hpp"
#include "firmware/motor_hardware.h"
#include "flex-stacker/motor_driver_task.hpp"
#include "flex-stacker/tmc2160_interface.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"

namespace motor_driver_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::MotorDriverQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Motor Driver Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = motor_driver_task::MotorDriverTask(_queue, nullptr);

static constexpr uint32_t STREAM_TASK_DEPTH = 200;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t stream_task_buffer;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, STREAM_TASK_DEPTH> stream_task_stack;
static constexpr uint32_t FREQ_MS = 10;

static void run_stallguard_task(void* arg) {
    auto* interface = static_cast<
        tmc2160::TMC2160Interface<motor_driver_policy::MotorDriverPolicy>*>(
        arg);

    uint32_t ulNotifiedValue = 0;
    MotorID motor_id = MotorID::MOTOR_X;

    xTaskNotifyWait(0, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);
    motor_id = (ulNotifiedValue == 1)   ? MotorID::MOTOR_X
               : (ulNotifiedValue == 2) ? MotorID::MOTOR_Z
                                        : MotorID::MOTOR_L;
    static_cast<void>(interface->read_stallguard(motor_id));
    for (;;) {
        if (xTaskNotifyWait(0, ULONG_MAX, &ulNotifiedValue, 0) == pdPASS) {
            // value changed
            motor_id = (ulNotifiedValue == 1)   ? MotorID::MOTOR_X
                       : (ulNotifiedValue == 2) ? MotorID::MOTOR_Z
                                                : MotorID::MOTOR_L;
            // restart first reading
            static_cast<void>(interface->read_stallguard(motor_id));
        } else {
            auto result = interface->read_stallguard(motor_id);
            auto msg = messages::StallGuardResultMessage{.data = result};
            static_cast<void>(_queue.try_send_from_isr(msg));
        }
        vTaskDelay(pdMS_TO_TICKS(FREQ_MS));
    }
}

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    spi_hardware_init();

    auto policy = motor_driver_policy::MotorDriverPolicy();
    auto tmc2160_interface = tmc2160::TMC2160Interface(policy);

    auto* stream_handle = xTaskCreateStatic(
        run_stallguard_task, "Stallguard Task", STREAM_TASK_DEPTH,
        &tmc2160_interface, 1, stream_task_stack.data(), &stream_task_buffer);
    vTaskSuspend(stream_handle);
    _top_task.provide_stallguard_handle(stream_handle);

    while (true) {
        _top_task.run_once(tmc2160_interface);
    }
}

};  // namespace motor_driver_task
