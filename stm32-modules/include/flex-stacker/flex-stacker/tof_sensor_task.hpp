
#include "tmf8820_spadmaps.hpp"
#pragma GCC push_options
#pragma GCC optimize("O0")

#include <array>
#include <cstdint>
#include <cstdio>

#include "core/fixed_point.hpp"
#include "core/queue_aggregator.hpp"
#include "flex-stacker/errors.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"
#include "flex-stacker/tmf8820.hpp"
#include "flex-stacker/tmf8820_registers.hpp"
#include "flex-stacker/tmf8820_spadmaps.hpp"
#include "hal/message_queue.hpp"
#include "hardware_iface.hpp"
#include "systemwide.h"
#include "tmf8820.hpp"
#include "tmf8820_registers.hpp"
#include "tof_sensor_hardware.h"
#include "tof_sensor_policy.hpp"

namespace tof_sensor_task {
using namespace tmf8820;
using namespace tof::hardware;
using Message = messages::TOFSensorMessage;

struct TOFSensor {
    TOFSensorID kind = TOF_NONE;
    TOFSensorMode mode = UNKNOWN;
    TOFSensorState state = DISABLED;
    tmf8820::TMF8820 driver;
    tmf8820::TMF8820Config config;
    bool ok = false;
};

tmf8820::TMF8820RegisterMap tof_x_config{};
tmf8820::TMF8820RegisterMap tof_z_config{};

const TOFSensor tof_sensor_x = {
    .kind = TOF_X,
    .driver = tmf8820::TMF8820(),
    .config =
        {
            .registers = &tof_x_config,
            .spad_config = &SPADConfigX,
        },
};

const TOFSensor tof_sensor_z = {
    .kind = TOF_Z,
    .driver = tmf8820::TMF8820(),
    .config =
        {
            .registers = &tof_z_config,
            .spad_config = &SPADConfigZ,
        },
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
        : _message_queue(q), _task_registry(aggregator), _policy(policy) {}
    TOFSensorTask(const TOFSensorTask& other) = delete;
    auto operator=(const TOFSensorTask& other) -> TOFSensorTask& = delete;
    TOFSensorTask(TOFSensorTask&& other) noexcept = delete;
    auto operator=(TOFSensorTask&& other) noexcept -> TOFSensorTask& = delete;
    ~TOFSensorTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <tmf8820::TMF8820Policy Policy>
    auto run_once(Policy* policy) -> void {
        if (!_task_registry) {
            return;
        }

        if (!_initialized) {
            _policy = policy;

            // Disable both sensors before initializing
            if (!_tof_sensor_x.ok) {
                TOFSensorPolicy::enable_tof_sensor(TOF_X, false);
            }
            if (!_tof_sensor_z.ok) {
                TOFSensorPolicy::enable_tof_sensor(TOF_Z, false);
            }

            for (auto sensor_id : {TOF_X, TOF_Z}) {
                auto sensor = &get_sensor(sensor_id);
                sensor->state = INITIALIZING;
                if (!sensor->ok) {
                    sensor->ok = sensor->driver.initialize(&sensor->config,
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
                .data = static_cast<uint8_t>(data.value()),
            };
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::SetTOFRegisterMessage& m) -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        auto driver = get_sensor(m.sensor_id).driver;
        auto data =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
            driver.write(m.sensor_id, m.reg, const_cast<uint8_t*>(&m.data), 1);
        if (!data.has_value()) {
            response.with_error = errors::ErrorCode::TMC2160_WRITE_ERROR;
        }
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::EnableTOFSensorMessage& m) -> void {
        auto response = messages::AcknowledgePrevious{.responding_to_id = m.id};
        auto sensor = &get_sensor(m.sensor_id);
        TOFSensorPolicy::enable_tof_sensor(m.sensor_id, m.enable);
        sensor->driver.reset_custom_address();
        sensor->state = DISABLED;
        sensor->ok = false;
        if (m.enable) {
            // Initialize takes 10s of seconds.
            sensor->state = INITIALIZING;
            sensor->ok = sensor->driver.initialize(&sensor->config, _policy,
                                                   sensor->kind);
            sensor->state = sensor->ok ? IDLE : TOF_ERROR;
        }
        sensor->mode = sensor->driver.get_sensor_mode(sensor->kind);
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto visit_message(const messages::GetTOFHistogramMessage& m) -> void {
        messages::HostCommsMessage response;
        auto sensor = &get_sensor(m.sensor_id);
        auto [ret, data] = sensor->driver.get_histogram_chunk(m.sensor_id);
        if (ret != HIST_OK) {
            response = messages::ErrorMessage{
                // TODO: add unique error
                .code = errors::ErrorCode::TMC2160_READ_ERROR};
            static_cast<void>(_task_registry->send_to_address(
                response, Queues::HostCommsAddress));
            return;
        }

        // Send the data
        response = messages::GetTOFHistogramResponse{
            .responding_to_id = m.id,
            .sensor_id = m.sensor_id,
            .len = 10,
            .end = true,
            .data = data.value().data(),
        };
        static_cast<void>(_task_registry->send_to_address(
            response, Queues::HostCommsAddress));
    }

    auto get_sensor(TOFSensorID sensor_id) -> TOFSensor& {
        switch (sensor_id) {
            case TOF_X:
                return _tof_sensor_x;
            case TOF_Z:
                return _tof_sensor_z;
            default:
                return _tof_sensor_x;
        }
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    TOFSensorPolicy* _policy;

    TOFSensor _tof_sensor_x = tof_sensor_x;
    TOFSensor _tof_sensor_z = tof_sensor_z;
    bool _initialized = false;
};
};  // namespace tof_sensor_task
#pragma GCC pop_options
