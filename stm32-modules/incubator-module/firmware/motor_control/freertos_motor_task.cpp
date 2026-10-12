#include <cstdint>

#include "FreeRTOS.h"
#include "firmware/firmware_tasks.hpp"
#include "firmware/freertos_tasks.hpp"
#include "firmware/motor_hardware.h"
#include "firmware/motor_policy.hpp"
#include "incubator-module/messages.hpp"
#include "incubator-module/motor_task.hpp"
#include "motor_interrupt.hpp"
#include "stm32g4xx_it.h"
#include "systemwide.h"
#include "task.h"

extern "C" {
void incubator_motor_on_step(MotorID motor_id);
}

namespace motor_control_task {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto r_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_R,
                                                         nullptr);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto z_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_Z,
                                                         nullptr);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto p_motor_interrupt =
    motor_interrupt_controller::MotorInterruptController(MotorID::MOTOR_P,
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
    _queue, nullptr, r_motor_interrupt, z_motor_interrupt, p_motor_interrupt);

auto on_step(MotorID motor_id) -> void {
    bool done = false;
    switch (motor_id) {
        case MotorID::MOTOR_R:
            done = r_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_Z:
            done = z_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_P:
            done = p_motor_interrupt.tick();
            break;
        default:
            break;
    }
    if (done) {
        static_cast<void>(_queue.try_send_from_isr(
            messages::MoveCompleteMessage{.motor_id = motor_id}));
    }
}

[[nodiscard]] static auto callback_glue(MotorID motor_id) {
    bool done = false;
    switch (motor_id) {
        case MotorID::MOTOR_R:
            done = r_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_Z:
            done = z_motor_interrupt.tick();
            break;
        case MotorID::MOTOR_P:
            done = p_motor_interrupt.tick();
            break;
        default:
            break;
    }
    if (done) {
        static_cast<void>(_queue.try_send_from_isr(
            messages::MoveCompleteMessage{.motor_id = motor_id}));
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
    while (true) {
        _top_task.run_once(policy);
    }
}

}  // namespace motor_control_task

extern "C" void incubator_motor_on_step(MotorID motor_id) {
    motor_control_task::on_step(motor_id);
}
