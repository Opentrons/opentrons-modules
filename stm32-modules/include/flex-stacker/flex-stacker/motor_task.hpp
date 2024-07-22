/**
 * @file motor_task.hpp
 * @brief Primary interface for the motor task
 *
 */
#pragma once

#include "core/ack_cache.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "flex-stacker/tmc2160_registers.hpp"
#include "hal/message_queue.hpp"

namespace motor_task {

using Message = messages::MotorMessage;

static constexpr tmc2160::TMC2160RegisterMap motor_z_config{
    .gconfig = {.diag0_error = 1, .diag1_stall = 1},
    .ihold_irun = {.hold_current = 1,
                   .run_current = 31,
                   .hold_current_delay = 1},
    .tcoolthrs = {.threshold = 0x81},
    .thigh = {.threshold = 0x81},
    .tpwmthrs = {.threshold = 0x80000},
    .chopconf = {.toff = 0b111,
                 .hstrt = 0b100,
                 .hend = 0b11,
                 .tbl = 0b1,
                 .mres = 0b100},
    .coolconf = {.semin = 0b11, .semax = 0b100},
    .pwmconf = {.pwm_ofs = 0x1F,
                .pwm_grad = 0x18,
                .pwm_autoscale = 1,
                .pwm_autograd = 1,
                .pwm_reg = 4,
                .pwm_lim = 0xC},
    .glob_scale = {.global_scaler = 0x8B},
};

static constexpr tmc2160::TMC2160RegisterMap motor_x_config{
    .gconfig = {.diag0_error = 1, .diag1_stall = 1},
    .ihold_irun = {.hold_current = 1,
                   .run_current = 31,
                   .hold_current_delay = 1},
    .tcoolthrs = {.threshold = 0x81},
    .thigh = {.threshold = 0x81},
    .tpwmthrs = {.threshold = 0x80000},
    .chopconf = {.toff = 0b111,
                 .hstrt = 0b111,
                 .hend = 0b1001,
                 .tbl = 0b1,
                 .mres = 0b100},
    .coolconf = {.semin = 0b11, .semax = 0b100},
    .pwmconf = {.pwm_ofs = 0x1F,
                .pwm_grad = 0x18,
                .pwm_autoscale = 1,
                .pwm_autograd = 1,
                .pwm_reg = 4,
                .pwm_lim = 0xC},
    .glob_scale = {.global_scaler = 0x8B},
};

static constexpr tmc2160::TMC2160RegisterMap motor_l_config{
    .gconfig = {.diag0_error = 1, .diag1_stall = 1},
    .ihold_irun = {.hold_current = 1,
                   .run_current = 31,
                   .hold_current_delay = 1},
    .tcoolthrs = {.threshold = 0x81},
    .thigh = {.threshold = 0x81},
    .tpwmthrs = {.threshold = 0x80000},
    .chopconf = {.toff = 0b111,
                 .hstrt = 0b111,
                 .hend = 0b1001,
                 .tbl = 0b1,
                 .mres = 0b100},
    .coolconf = {.semin = 0b11, .semax = 0b100},
    .pwmconf = {.pwm_ofs = 0x1F,
                .pwm_grad = 0x18,
                .pwm_autoscale = 1,
                .pwm_autograd = 1,
                .pwm_reg = 4,
                .pwm_lim = 0xC},
    .glob_scale = {.global_scaler = 0x8B},
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit MotorTask(Queue& q, Aggregator* aggregator)
        : _message_queue(q), _task_registry(aggregator) {}
    MotorTask(const MotorTask& other) = delete;
    auto operator=(const MotorTask& other) -> MotorTask& = delete;
    MotorTask(MotorTask&& other) noexcept = delete;
    auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
    ~MotorTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    auto run_once() -> void {
        if (!_task_registry) {
            return;
        }


        auto message = Message(std::monostate());

        _message_queue.recv(&message);
        auto visit_helper = [this](auto& message) -> void {
            this->visit_message(message);
        };
        std::visit(visit_helper, message);
    }

  private:
    auto visit_message(const std::monostate& message) -> void {
        static_cast<void>(message);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized;
};

};  // namespace motor_task
