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

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static tasks::FirmwareTasks::MotorQueue
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
           "Motor Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task =
    motor_task::MotorTask(_queue, nullptr, r_motor_interrupt);

auto on_step(MotorID motor_id) -> void {
    bool done = false;
    if (motor_id == MotorID::MOTOR_R) {
        done = r_motor_interrupt.tick();
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
    initialize_callbacks(incubator_motor_on_step);
    auto policy = motor_policy::MotorPolicy();
    while (true) {
        _top_task.run_once(policy);
    }
}

}  // namespace motor_control_task

extern "C" void incubator_motor_on_step(MotorID motor_id) {
    motor_control_task::on_step(motor_id);
}
