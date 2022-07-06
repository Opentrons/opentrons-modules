#pragma once

#include "hal/message_queue.hpp"
#include "tempdeck-gen3/messages.hpp"
#include "tempdeck-gen3/tasks.hpp"

namespace thermal_task {

template <typename Policy>
concept ThermalPolicy = requires(Policy& p) {
    {p()};
};

using Message = messages::ThermalMessage;

struct ThermalReadings {
    uint32_t plate_adc = 0;
    uint32_t heatsink_adc = 0;
    uint32_t last_tick = 0;
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class ThermalTask {
  public:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

    explicit ThermalTask(Queue& q, Aggregator* aggregator)
        : _message_queue(q),
          _task_registry(aggregator),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _readings() {}
    ThermalTask(const ThermalTask& other) = delete;
    auto operator=(const ThermalTask& other) -> ThermalTask& = delete;
    ThermalTask(ThermalTask&& other) noexcept = delete;
    auto operator=(ThermalTask&& other) noexcept -> ThermalTask& = delete;
    ~ThermalTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <ThermalPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }

        auto message = Message(std::monostate());
        _message_queue.recv(&message);
        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

    [[nodiscard]] auto get_readings() const -> ThermalReadings {
        return _readings;
    }

  private:
    template <ThermalPolicy Policy>
    auto visit_message(const std::monostate& message, Policy& policy) -> void {
        static_cast<void>(message);
        static_cast<void>(policy);
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::ThermistorReadings& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        _readings.heatsink_adc = message.heatsink;
        _readings.plate_adc = message.plate;
        _readings.last_tick = message.timestamp;
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    ThermalReadings _readings;
};

};  // namespace thermal_task
