#pragma once

#include <stdio.h>

#include <array>
#include <cstdint>

#include "core/ack_cache.hpp"
#include "core/fixed_point.hpp"
#include "core/queue_aggregator.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "flex-stacker/tmf8821.hpp"
#include "flex-stacker/tmf8821_registers.hpp"
#include "hal/message_queue.hpp"
#include "hardware_iface.hpp"
#include "messages.hpp"
#include "systemwide.h"
#include "tof_driver_policy.hpp"
#include "tof_sensor_hardware.h"

/*
TODO:
- need to add way to `initialize` the sensors
    - enable the sensor
    - wrte 0x01 to ENABLE (0xE0) reg
    - read the ENABLE (0xE0) reg until result = 0x41
    - read APPID (0x00) reg
    - if APPID = 0x80 do bootloader routine
    - if APPID = 0x03 do measureument routine      <--------- INITIALIZED?

- need to handle APPID = 0x80 bootloader routine
    - figure out how to store app image in Flash
    - add mechanism to detect bootloader mode
    - add mechanism to determine if an update is required
    - add mechanism to send app image in Flash to sensor
    - add mechanism to verify the update is complete
    - add mechanism to transition from bootloader mode to Factory Mode or Main
App Mode

- need to handle APPID = 0x03 Main App Mode
    - add mechanism to detect we are in Main App Mode
    - add mechanism to get/set the measurement configs
    - add mechanism to get/set the calibration

- need to handle APPID = 0x03, CIDRID = 0x19 Factory Mode
    - add mechanism to detect we are in Factory Mode
    - add mechanism to set/get factory calibration
*/

// This is the default address, this will be changed in sw.
static constexpr const uint8_t TOF_DEFAULT_ADDR = (0x41)
                                                  << 1;  // 0x41 DEFAULT address

namespace tof_driver_task {
using namespace tof::hardware;
using namespace tmf8821;
using Message = messages::TOFDriverMessage;


static constexpr tmf8821::TMF8821RegisterMap tof_x_config{
    // TODO: Change these defaults once available
    .enable = {.pon = 1, .powerup_select = 0}
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class TOFDriverTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit TOFDriverTask(Queue& q, Aggregator* aggregator,
                           TOFDriverPolicy* policy)
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

    template <tmf8821::TMF8821Policy Policy>
    auto run_once(Policy* policy) -> void {
        if (!_task_registry) {
            return;
        }

        if (!_initialized) {
            _policy = policy;
            auto s1 = _tmf8821.initialize_sensor(_tof_x_config, _policy, TOF_X);
            static_cast<void>(s1);
            //auto s2 = _tmf8821.initialize(_policy, TOF_Z);
            // sensor_status = SensorStatus(ret1, ret2)
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
            response = messages::ErrorMessage{
                .code = errors::ErrorCode::TMC2160_READ_ERROR};
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

        auto [res, data] =
            _policy->i2c_write(TOF_DEFAULT_ADDR, reg, &value, size);
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
    TOFDriverPolicy* _policy;
    bool _initialized = false;

    tmf8821::TMF8821 _tmf8821{};
    tmf8821::TMF8821RegisterMap _tof_x_config = tof_x_config;
};
};  // namespace tof_driver_task
