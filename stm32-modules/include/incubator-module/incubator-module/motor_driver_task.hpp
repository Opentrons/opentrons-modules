/**
 * @file motor_driver_task.hpp
 * @brief Configures the TMC5160 and services driver register messages
 *
 */
#pragma once

#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "errors.hpp"
#include "firmware/motor_policy.hpp"
#include "incubator-module/errors.hpp"
#include "incubator-module/messages.hpp"
#include "incubator-module/tasks.hpp"
#include "incubator-module/tmc5160.hpp"
#include "incubator-module/tmc5160_interface.hpp"
#include "incubator-module/tmc5160_registers.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "systemwide.h"

namespace motor_driver_task {

using Message = messages::MotorDriverMessage;

static constexpr tmc5160::TMC5160RegisterMap motor_r_config{
    .gconfig = {.diag0_error = 0, .diag0_stall = 0},
    .short_conf = {.s2vs_level = 0x6,
                   .s2g_level = 0x6,
                   .shortfilter = 1,
                   .shortdelay = 0},
    .drvconf = {.bbmclks = 4},
    .glob_scale = {.global_scaler = 0x0},
    .ihold_irun = {.hold_current = 7,
                   .run_current = 31,
                   .hold_current_delay = 1},
    .tpowerdown = {.time = tmc5160::PowerDownDelay::seconds_to_reg(0.1)},
    .tpwmthrs = {.threshold = 0x80000},
    .tcoolthrs = {.threshold = 0x2FF},
    .thigh = {.threshold = 0x1},
    .chopconf = {.toff = 0b111,
                 .hstrt = 0b111,
                 .hend = 0b1001,
                 .tbl = 0b1,
                 .mres = 0b100},
    .coolconf = {.semin = 0b11, .semax = 0b100, .sgt = 0},
    .pwmconf = {.pwm_ofs = 0x1F,
                .pwm_grad = 0x18,
                .pwm_autoscale = 1,
                .pwm_autograd = 1,
                .pwm_reg = 4,
                .pwm_lim = 0xC},
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class MotorDriverTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit MotorDriverTask(Queue& q, Aggregator* aggregator)
        : _message_queue(q), _task_registry(aggregator) {}
    MotorDriverTask(const MotorDriverTask& other) = delete;
    auto operator=(const MotorDriverTask& other) -> MotorDriverTask& = delete;
    MotorDriverTask(MotorDriverTask&& other) noexcept = delete;
    auto operator=(MotorDriverTask&& other) noexcept
        -> MotorDriverTask& = delete;
    ~MotorDriverTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    auto driver_conf_from_id(MotorID motor_id) -> tmc5160::TMC5160RegisterMap& {
        switch (motor_id) {
            case MotorID::MOTOR_R:  return _r_config;
            default:
                return _r_config;
        }
    }

    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }
        auto tmc5160_interface = tmc5160::TMC5160Interface<Policy>(policy);
        if (!_initialized) {
            if (!_tmc5160.initialize_config(motor_r_config, tmc5160_interface,
                                            MotorID::MOTOR_R)) {
                return;
            }
            _message_queue.set_ready();
            _initialized = true;
        }

        auto message = Message(std::monostate());

        _message_queue.recv(&message);
        auto visit_helper = [this, &tmc5160_interface](auto& message) -> void {
            this->visit_message(message, tmc5160_interface);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto visit_message(const std::monostate& m,
                       tmc5160::TMC5160Interface<Policy>& tmc5160_interface)
        -> void {
        static_cast<void>(m);
        static_cast<void>(tmc5160_interface);
    }

    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto visit_message(const messages::SetTMCRegisterMessage& m,
                       tmc5160::TMC5160Interface<Policy>& tmc5160_interface)
        -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        if (!tmc5160::is_valid_address(m.reg)) {
            response.with_error = errors::ErrorCode::TMC5160_INVALID_ADDRESS;
        } else {
            auto result = tmc5160_interface.write(tmc5160::Registers(m.reg),
                                                  m.data, m.motor_id);
            if (!result) {
                response.with_error = errors::ErrorCode::TMC5160_WRITE_ERROR;
            }
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto visit_message(const messages::GetTMCRegisterMessage& m,
                       tmc5160::TMC5160Interface<Policy>& tmc5160_interface)
        -> void {
        messages::HostCommsMessage response;
        if (tmc5160::is_valid_address(m.reg)) {
            auto data =
                tmc5160_interface.read(tmc5160::Registers(m.reg), m.motor_id);
            if (!data.has_value()) {
                response = messages::ErrorMessage{
                    .code = errors::ErrorCode::TMC5160_READ_ERROR};
            } else {
                response =
                    messages::GetTMCRegisterResponse{.responding_to_id = m.id,
                                                     .motor_id = m.motor_id,
                                                     .reg = m.reg,
                                                     .data = data.value()};
            }

            static_cast<void>(_task_registry->send_to_address(
                response, Queues::HostCommsAddress));
        }
    }

    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto visit_message(const messages::PollTMCRegisterMessage& m,
                       tmc5160::TMC5160Interface<Policy>& tmc5160_interface)
        -> void {
        static_cast<void>(m);
        static_cast<void>(tmc5160_interface);
    }

    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto visit_message(const messages::StopPollTMCRegisterMessage& m,
                       tmc5160::TMC5160Interface<Policy>& tmc5160_interface)
        -> void {
        static_cast<void>(m);
        static_cast<void>(tmc5160_interface);
    }

    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto visit_message(const messages::SetMicrostepsMessage& m,
                       tmc5160::TMC5160Interface<Policy>& tmc5160_interface)
        -> void {
        driver_conf_from_id(m.motor_id).chopconf.mres = m.microsteps_power;
        if (!_tmc5160.update_chopconf(driver_conf_from_id(m.motor_id),
                                      tmc5160_interface, m.motor_id)) {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = m.id,
                .with_error = errors::ErrorCode::TMC5160_WRITE_ERROR};
            static_cast<void>(_task_registry->send_to_address(
                response, Queues::HostCommsAddress));
            return;
        };
        static_cast<void>(
            _task_registry->send_to_address(m, Queues::MotorAddress));
    }

    template <tmc5160::TMC5160InterfacePolicy Policy>
    auto visit_message(const messages::SetMotorCurrentMessage& m,
                       tmc5160::TMC5160Interface<Policy>& tmc5160_interface)
        -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        if (m.hold_current != 0.0) {
            driver_conf_from_id(m.motor_id).ihold_irun.hold_current =
                get_current_value(m.motor_id, m.hold_current);
        };
        if (m.run_current != 0.0) {
            driver_conf_from_id(m.motor_id).ihold_irun.run_current =
                get_current_value(m.motor_id, m.run_current);
        }
        if (!_tmc5160.update_current(driver_conf_from_id(m.motor_id),
                                     tmc5160_interface, m.motor_id)) {
            response.with_error = errors::ErrorCode::TMC5160_WRITE_ERROR;
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto get_current_value(MotorID motor_id, float current) -> uint32_t {
        return _tmc5160.convert_peak_current_to_tmc5160_value(
            current, driver_conf_from_id(motor_id).glob_scale,
            _motor_current_config);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized{false};

    tmc5160::TMC5160 _tmc5160{};
    const tmc5160::TMC5160MotorCurrentConfig _motor_current_config{
        .r_sense = 0.15,
        .v_sf = 0.325,
    };
    tmc5160::TMC5160RegisterMap _r_config = motor_r_config;
};
}  // namespace motor_driver_task
