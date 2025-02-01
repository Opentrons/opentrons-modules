#include <stdio.h>

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
#include "systemwide.h"
#include "tmf8821.hpp"
#include "tof_sensor_policy.hpp"
#include "tof_sensor_hardware.h"


namespace tof_sensor_task {
using namespace tof::hardware;
using namespace tmf8821;
using Message = messages::TOFSensorMessage;

static constexpr tmf8821::TMF8821RegisterMap tof_x_config{
    // TODO: Change these defaults once available
    .enable = {.pon = 0, .powerup_select = 0}};

static constexpr tmf8821::TMF8821RegisterMap tof_z_config{
    // TODO: Change these defaults once available
    .enable = {.pon = 0, .powerup_select = 0}};

struct TOFSensor {
    TOFSensorID kind = TOF_NONE;
    TOFSensorMode mode = UNKNOWN;
    TOFSensorState state = DISABLED;
    tmf8821::TMF8821 driver{nullptr};
    tmf8821::TMF8821RegisterMap config;
    bool ok = false;
};

const TOFSensor tof_sensor_x = {
    .kind = TOF_X,
    .driver = tmf8821::TMF8821{nullptr},
    .config = tof_x_config,
};

const TOFSensor tof_sensor_z = {
    .kind = TOF_Z,
    .driver = tmf8821::TMF8821{nullptr},
    .config = tof_z_config,
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class TOFSensorTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit TOFSensorTask(Queue& q, Aggregator* aggregator,
                           TOFSensorPolicy* policy)
        : _message_queue(q),
          _task_registry(aggregator),
          _policy(policy),
          _initialized(false) {}
    TOFSensorTask(const TOFSensorTask& other) = delete;
    auto operator=(const TOFSensorTask& other) -> TOFSensorTask& = delete;
    TOFSensorTask(TOFSensorTask&& other) noexcept = delete;
    auto operator=(TOFSensorTask&& other) noexcept -> TOFSensorTask& = delete;
    ~TOFSensorTask() = default;

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

            // Disable both sensors before initializing
            if (!_tof_sensor_x.ok) _policy->enable_tof_sensor(TOF_X, false);
            if (!_tof_sensor_z.ok) _policy->enable_tof_sensor(TOF_Z, false);

            for (auto sensor_id : {TOF_X, TOF_Z}) {
                auto sensor = &get_sensor(sensor_id);
                sensor->state = INITIALIZING;
                if (!sensor->ok) {
                    sensor->ok = sensor->driver.initialize(sensor->config,
                                                           _policy, sensor_id);
                    sensor->state = sensor->ok ? IDLE : TOF_ERROR;
                }
                sensor->mode = sensor->driver.get_sensor_mode(sensor_id);
            }
            _initialized = _tof_sensor_x.ok && _tof_sensor_z.ok;
            auto message = messages::SetStatusBarStateMessage{
                .color = _initialized ? Green : Red, .pattern = Static};
            static_cast<void>(
                _task_registry->send_to_address(message, Queues::UIAddress));
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

    auto visit_message(const messages::GetTOFSensorStatusMessage& m) -> void {
        auto sensor = get_sensor(m.sensor_id);
        auto response = messages::GetTOFSensorStatusResponse{
            .responding_to_id = m.id,
            .sensor_id = m.sensor_id,
            .state = sensor.state,
            .mode = sensor.mode,
            .ok = sensor.ok,
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::GetTOFRegisterMessage& m) -> void {
        messages::HostCommsMessage response;
        auto driver = get_sensor(m.sensor_id).driver;
        auto data = driver.read(m.sensor_id, m.reg, 1);
        if (!data.has_value()) {
            response = messages::ErrorMessage{
                .code = errors::ErrorCode::TMC2160_READ_ERROR};
        } else {
            response = messages::GetTOFRegisterResponse{
                .responding_to_id = m.id,
                .sensor_id = m.sensor_id,
                .reg = m.reg,
                .data = data.value(),
            };
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::SetTOFRegisterMessage& m) -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        auto reg = static_cast<uint16_t>(m.reg);
        auto value = static_cast<uint32_t>(m.data);
        auto driver = get_sensor(m.sensor_id).driver;
        auto data = driver.write(m.sensor_id, reg, &value, 1);
        if (!data.has_value()) {
            response.with_error = errors::ErrorCode::TMC2160_WRITE_ERROR;
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::EnableTOFSensorMessage& m) -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        auto sensor = &get_sensor(m.sensor_id);
        _policy->enable_tof_sensor(m.sensor_id, m.enable);
        sensor->driver.reset_custom_address();
        sensor->state = DISABLED;
        sensor->ok = false;
        if (m.enable) {
            // Initialize takes 10s of seconds.
            sensor->state = INITIALIZING;
            sensor->ok = sensor->driver.initialize(sensor->config, _policy,
                                                   sensor->kind);
            sensor->state = sensor->ok ? IDLE : TOF_ERROR;
        }
        sensor->mode = sensor->driver.get_sensor_mode(sensor->kind);
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto get_sensor(TOFSensorID sensor_id) -> TOFSensor& {
        if (sensor_id == TOF_X) return _tof_sensor_x;
        if (sensor_id == TOF_Z) return _tof_sensor_z;
        return _tof_sensor_x;
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    TOFSensorPolicy* _policy;

    TOFSensor _tof_sensor_x = tof_sensor_x;
    TOFSensor _tof_sensor_z = tof_sensor_z;
    bool _initialized = false;
};
};  // namespace tof_sensor_task
