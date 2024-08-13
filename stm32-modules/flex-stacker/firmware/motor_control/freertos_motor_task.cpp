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
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_X);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto z_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_Z);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto l_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_L);

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::MotorQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Motor Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = motor_task::MotorTask(_queue, nullptr);

static auto callback_glue(MotorID motor_id) {
    switch (motor_id) {
        case MotorID::MOTOR_L:
            l_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_X:
            x_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_Z:
            z_motor_interrupt.tick();
            break;
        default:
            break;
    }
}

auto run(tasks::FirmwareTasks::QueueAggregator* aggregator) -> void {
    auto* handle = xTaskGetCurrentTaskHandle();
    _queue.provide_handle(handle);
    aggregator->register_queue(_queue);
    _top_task.provide_aggregator(aggregator);

    motor_hardware_init();
    initialize_callbacks(callback_glue);
    auto policy = motor_policy::MotorPolicy();
    policy.enable_motor(MotorID::MOTOR_L);
    policy.enable_motor(MotorID::MOTOR_X);
    policy.enable_motor(MotorID::MOTOR_Z);
    while (true) {
        _top_task.run_once(policy);
    }
}

};  // namespace motor_control_task
