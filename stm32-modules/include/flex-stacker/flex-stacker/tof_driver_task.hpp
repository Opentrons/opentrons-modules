#pragma once

#include <cstdint>

#include "core/ack_cache.hpp"
#include "core/fixed_point.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "errors.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "hal/message_queue.hpp"
#include "i2c_comms.hpp"
#include "messages.hpp"
#include "systemwide.h"

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
        // FOR TESTING
        // reg, write_flag, data
        MessageT msg = {m.reg, 0x00, 0x00};  // read from APPID Reg (0x00)
        auto response = messages::GetTOFRegisterResponse{
            .responding_to_id = m.id,
            .sensor_id = m.sensor_id,
            .reg = 0xff,
            .data = 0xffff,
        };
        //
        auto resp = _policy->transmit_receive(0x44, msg, true);
        if (resp.has_value()) {
            response.reg = m.reg;
            response.data = static_cast<uint32_t>(*resp.value().data());
        }

        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::SetTOFRegisterMessage& m) -> void {
        static_cast<void>(m);
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    i2c::hardware::I2C* _policy;
    bool _initialized = false;
};
};  // namespace tof_driver_task
