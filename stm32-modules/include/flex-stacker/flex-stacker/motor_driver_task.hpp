/**
 * @file motor_task.hpp
 * @brief Primary interface for the motor task
 *
 */
#pragma once

#include "core/ack_cache.hpp"
#include "core/fixed_point.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "errors.hpp"
#include "firmware/motor_policy.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "flex-stacker/tmc2160.hpp"
#include "flex-stacker/tmc2160_interface.hpp"
#include "flex-stacker/tmc2160_registers.hpp"
#include "hal/message_queue.hpp"
#include "messages.hpp"
#include "systemwide.h"

namespace motor_driver_task {

using Message = messages::MotorDriverMessage;

static constexpr tmc2160::TMC2160RegisterMap motor_z_config{
    .gconfig = {.diag0_error = 0, .diag0_stall = 0},
    .short_conf = {.s2vs_level = 0x6,
                   .s2g_level = 0x6,
                   .shortfilter = 1,
                   .shortdelay = 0},
    .glob_scale = {.global_scaler = 0x0},
    .ihold_irun = {.hold_current = 10,
                   .run_current = 31,
                   .hold_current_delay = 15},
    .tpwmthrs = {.threshold = 0x80000},
    .tcoolthrs = {.threshold = 0x2FF},
    .thigh = {.threshold = 0x81},
    .chopconf = {.toff = 0b111,
                 .hstrt = 0b100,
                 .hend = 0b11,
                 .tbl = 0b1,
                 .mres = 0b100},
    .coolconf = {.semin = 0b11, .semax = 0b100, .sgt = 1},
    .pwmconf = {.pwm_ofs = 0x1F,
                .pwm_grad = 0x18,
                .pwm_autoscale = 1,
                .pwm_autograd = 1,
                .pwm_reg = 4,
                .pwm_lim = 0xC},
};

static constexpr tmc2160::TMC2160RegisterMap motor_x_config{
    .gconfig = {.diag0_error = 0, .diag0_stall = 0},
    .short_conf = {.s2vs_level = 0x6,
                   .s2g_level = 0x6,
                   .shortfilter = 1,
                   .shortdelay = 0},
    .glob_scale = {.global_scaler = 0x0},
    .ihold_irun = {.hold_current = 12,
                   .run_current = 31,
                   .hold_current_delay = 7},
    .tpwmthrs = {.threshold = 0x80000},
    .tcoolthrs = {.threshold = 0x2FF},
    .thigh = {.threshold = 0x81},
    .chopconf = {.toff = 0b111,
                 .hstrt = 0b111,
                 .hend = 0b1001,
                 .tbl = 0b1,
                 .mres = 0b100},
    .coolconf = {.semin = 0b11, .semax = 0b100, .sgt = 1},
    .pwmconf = {.pwm_ofs = 0x1F,
                .pwm_grad = 0x18,
                .pwm_autoscale = 1,
                .pwm_autograd = 1,
                .pwm_reg = 4,
                .pwm_lim = 0xC},
};

static constexpr tmc2160::TMC2160RegisterMap motor_l_config{
    .gconfig = {.diag0_error = 0, .diag0_stall = 0},
    .short_conf = {.s2vs_level = 0x6,
                   .s2g_level = 0x6,
                   .shortfilter = 1,
                   .shortdelay = 0},
    .glob_scale = {.global_scaler = 0x0},
    .ihold_irun = {.hold_current = 7,
                   .run_current = 8,
                   .hold_current_delay = 7},
    .tpwmthrs = {.threshold = 0x80000},
    .tcoolthrs = {.threshold = 0x2FF},
    .thigh = {.threshold = 0x81},
    .chopconf = {.toff = 0b111,
                 .hstrt = 0b111,
                 .hend = 0b1001,
                 .tbl = 0b1,
                 .mres = 0b100},
    .coolconf = {.semin = 0b11, .semax = 0b100, .sgt = 1},
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
        : _message_queue(q), _task_registry(aggregator), _initialized(false) {}
    MotorDriverTask(const MotorDriverTask& other) = delete;
    auto operator=(const MotorDriverTask& other) -> MotorDriverTask& = delete;
    MotorDriverTask(MotorDriverTask&& other) noexcept = delete;
    auto operator=(MotorDriverTask&& other) noexcept
        -> MotorDriverTask& = delete;
    ~MotorDriverTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    auto driver_conf_from_id(MotorID motor_id) -> tmc2160::TMC2160RegisterMap& {
        switch (motor_id) {
            case MotorID::MOTOR_X:
                return _x_config;
            case MotorID::MOTOR_Z:
                return _z_config;
            case MotorID::MOTOR_L:
                return _l_config;
            default:
                return _x_config;
        }
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }
        auto tmc2160_interface = tmc2160::TMC2160Interface<Policy>(policy);
        if (!_initialized) {
            if (!_tmc2160.initialize_config(motor_x_config, tmc2160_interface,
                                            MotorID::MOTOR_X)) {
                return;
            }
            if (!_tmc2160.initialize_config(motor_z_config, tmc2160_interface,
                                            MotorID::MOTOR_Z)) {
                return;
            }
            if (!_tmc2160.initialize_config(motor_l_config, tmc2160_interface,
                                            MotorID::MOTOR_L)) {
                return;
            }
            _initialized = true;
        }

        auto message = Message(std::monostate());

        _message_queue.recv(&message);
        auto visit_helper = [this, &tmc2160_interface](auto& message) -> void {
            this->visit_message(message, tmc2160_interface);
        };
        std::visit(visit_helper, message);
    }

  private:
    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const std::monostate& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        static_cast<void>(m);
        static_cast<void>(tmc2160_interface);
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::SetTMCRegisterMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        if (!tmc2160::is_valid_address(m.reg)) {
            response.with_error = errors::ErrorCode::TMC2160_INVALID_ADDRESS;
        } else {
            auto result = tmc2160_interface.write(tmc2160::Registers(m.reg),
                                                  m.data, m.motor_id);
            if (!result) {
                response.with_error = errors::ErrorCode::TMC2160_WRITE_ERROR;
            }
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::GetTMCRegisterMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        messages::HostCommsMessage response;
        if (tmc2160::is_valid_address(m.reg)) {
            auto data =
                tmc2160_interface.read(tmc2160::Registers(m.reg), m.motor_id);
            if (!data.has_value()) {
                response = messages::ErrorMessage{
                    .code = errors::ErrorCode::TMC2160_READ_ERROR};
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

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::PollTMCRegisterMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        static_cast<void>(m);
        static_cast<void>(tmc2160_interface);
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::StopPollTMCRegisterMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        static_cast<void>(m);
        static_cast<void>(tmc2160_interface);
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::SetMicrostepsMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        driver_conf_from_id(m.motor_id).chopconf.mres = m.microsteps_power;
        if (!_tmc2160.update_chopconf(driver_conf_from_id(m.motor_id),
                                      tmc2160_interface, m.motor_id)) {
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = m.id,
                .with_error = errors::ErrorCode::TMC2160_WRITE_ERROR};
            static_cast<void>(_task_registry->send_to_address(
                response, Queues::HostCommsAddress));
            return;
        };
        static_cast<void>(
            _task_registry->send_to_address(m, Queues::MotorAddress));
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::SetMotorCurrentMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
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
        if (!_tmc2160.update_current(driver_conf_from_id(m.motor_id),
                                     tmc2160_interface, m.motor_id)) {
            response.with_error = errors::ErrorCode::TMC2160_WRITE_ERROR;
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::SetMotorStallGuardMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        static_cast<void>(tmc2160_interface);
        auto response = messages::AcknowledgePrevious{
            .responding_to_id = m.id,
            .with_error = errors::ErrorCode::NO_ERROR};

        if (_tmc2160.verify_sgt_value(m.sgt)) {
            driver_conf_from_id(m.motor_id).coolconf.sgt = m.sgt.value();
            driver_conf_from_id(m.motor_id).gconfig.diag0_stall =
                static_cast<int>(m.enable);
        } else {
            response.with_error = errors::ErrorCode::TMC2160_INVALID_VALUE;
        }
        if (!_tmc2160.update_coolconf(driver_conf_from_id(m.motor_id),
                                      tmc2160_interface, m.motor_id)) {
            response.with_error = errors::ErrorCode::TMC2160_WRITE_ERROR;
        }
        if (!_tmc2160.update_gconfig(driver_conf_from_id(m.motor_id),
                                     tmc2160_interface, m.motor_id)) {
            response.with_error = errors::ErrorCode::TMC2160_WRITE_ERROR;
        }

        if (response.with_error == errors::ErrorCode::NO_ERROR) {
            auto message = messages::SetDiag0IRQMessage{.enable = m.enable};
            static_cast<void>(
                _task_registry->send_to_address(message, Queues::MotorAddress));
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    template <tmc2160::TMC2160InterfacePolicy Policy>
    auto visit_message(const messages::GetMotorStallGuardMessage& m,
                       tmc2160::TMC2160Interface<Policy>& tmc2160_interface)
        -> void {
        static_cast<void>(tmc2160_interface);
        bool enabled = static_cast<bool>(
            driver_conf_from_id(m.motor_id).gconfig.diag0_stall);
        int sgt = driver_conf_from_id(m.motor_id).coolconf.sgt;
        auto response = messages::GetMotorStallGuardResponse{
            .id = m.id,
            .motor_id = m.motor_id,
            .enabled = enabled,
            .sgt = sgt,
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto get_current_value(MotorID motor_id, float current) -> uint32_t {
        return _tmc2160.convert_peak_current_to_tmc2160_value(
            current, driver_conf_from_id(motor_id).glob_scale,
            _motor_current_config);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    bool _initialized;

    tmc2160::TMC2160 _tmc2160{};
    // same motor current config for all three motors
    const tmc2160::TMC2160MotorCurrentConfig _motor_current_config{
        .r_sense = 0.22,
        .v_sf = 0.325,
    };
    tmc2160::TMC2160RegisterMap _x_config = motor_x_config;
    tmc2160::TMC2160RegisterMap _z_config = motor_z_config;
    tmc2160::TMC2160RegisterMap _l_config = motor_l_config;
};
};  // namespace motor_driver_task
