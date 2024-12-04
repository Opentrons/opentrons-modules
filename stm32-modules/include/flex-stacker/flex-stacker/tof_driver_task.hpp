#pragma once

#include <stdio.h>
#include <cstdint>
#include <array>

#include "core/ack_cache.hpp"
#include "core/fixed_point.hpp"
#include "core/queue_aggregator.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "hal/message_queue.hpp"
#include "i2c_comms.hpp"
#include "messages.hpp"
#include "systemwide.h"
#include "tof_sensor_hardware.h"

// This is the default address, this will be changed in sw.
static constexpr const uint8_t TOF_DEFAULT_ADDR = (0x41) << 1;  // 0x41 DEFAULT address

namespace tof_driver_task {
using namespace tof_driver_policy;
using Message = messages::TOFDriverMessage;

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class TOFDriverTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit TOFDriverTask(Queue& q, Aggregator* aggregator,
                           i2c::hardware::I2C* policy)
        : _message_queue(q),
          _task_registry(aggregator),
          _policy(policy),
          _initialized(false) {}
    TOFDriverTask(const TOFDriverTask& other) = delete;
    auto operator=(const TOFDriverTask& other) -> TOFDriverTask& = delete;
    TOFDriverTask(TOFDriverTask&& other) noexcept = delete;
    auto operator=(TOFDriverTask&& other) noexcept -> TOFDriverTask& = delete;
    ~TOFDriverTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    auto set_i2c_comms(i2c::hardware::I2C* policy) -> void { _policy = policy; }

    auto run_once() -> void {
        if (!_task_registry) {
            return;
        }

        if (!_initialized) {
            _initialized = true;
        }

        auto message = Message(std::monostate());
        _message_queue.recv(&message);

        auto visit_helper = [this](auto& message) -> void {
            this->visit_message(message);
        };
        std::visit(visit_helper, message);
    }

  private:
    auto visit_message(const std::monostate& m) -> void {
        static_cast<void>(m);
    }

    auto visit_message(const messages::GetTOFRegisterMessage& m) -> void {
        messages::HostCommsMessage response;

        // TODO: This should be done by some message builder
        auto reg = static_cast<uint16_t>(m.reg);
        auto size = 1;

        auto [res, data] = _policy->i2c_read(TOF_DEFAULT_ADDR, reg, size);
        if (res != 0) {
            response = messages::ErrorMessage{.code = errors::ErrorCode::TMC2160_READ_ERROR};
        } else {
            auto value = static_cast<uint32_t>(*data.data());
            response = messages::GetTOFRegisterResponse{
                .responding_to_id = m.id,
                .sensor_id = m.sensor_id,
                .reg = res,
                .data = value,
            };
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::SetTOFRegisterMessage& m) -> void {
        // TODO: This should be done by some message builder
        auto reg = static_cast<uint16_t>(m.reg);
        auto value = static_cast<uint8_t>(m.data);
        auto size = 1;

        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        // TODO: Validate register and value

        auto [res, data] = _policy->i2c_write(TOF_DEFAULT_ADDR, reg, &value, size);
        if (res != 0) {
            response.with_error = errors::ErrorCode::TMC2160_WRITE_ERROR;
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::EnableTOFSensorMessage& m) -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        // TODO: add return value
        enable_tof_sensor_write(m.sensor_id, m.enable);
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    i2c::hardware::I2C* _policy;
    bool _initialized = false;
};
};  // namespace tof_driver_task
