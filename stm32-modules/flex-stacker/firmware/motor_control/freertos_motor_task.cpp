#include "FreeRTOS.h"
#include "firmware/freertos_tasks.hpp"
#include "firmware/motor_hardware.h"
#include "firmware/motor_policy.hpp"
#include "flex-stacker/motor_task.hpp"
#include "motor_interrupt.hpp"
#include "ot_utils/freertos/freertos_timer.hpp"
#include "stm32g4xx_it.h"
#include "systemwide.h"

namespace motor_control_task {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto x_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_X,
                                                         nullptr);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto z_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_Z,
                                                         nullptr);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto l_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_L,
                                                         nullptr);

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::MotorQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Motor Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = motor_task::MotorTask(
    _queue, nullptr, x_motor_interrupt, z_motor_interrupt, l_motor_interrupt);

[[nodiscard]] static auto callback_glue(MotorID motor_id) {
    bool done = false;
    switch (motor_id) {
        case MotorID::MOTOR_L:
            done = l_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_X:
            done = x_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_Z:
            done = z_motor_interrupt.tick();
            break;
        default:
            break;
    }
    if (done) {
        static_cast<void>(_queue.try_send_from_isr(
            messages::MoveCompleteMessage{.motor_id = motor_id}));
    }
}

[[nodiscard]] static auto report_callback_glue(uint64_t step_count,
                                               uint64_t distance,
                                               uint32_t velocity) {
    static_cast<void>(_queue.try_send_from_isr(
        messages::MoveDebugMessage{.step_count = step_count,
                                   .distance = distance,
                                   .velocity = velocity}));
}

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    motor_hardware_init(report_callback_glue);
    initialize_callbacks(callback_glue);
    auto policy = motor_policy::MotorPolicy();
    while (true) {
        _top_task.run_once(policy);
    }
}

};  // namespace motor_control_task
