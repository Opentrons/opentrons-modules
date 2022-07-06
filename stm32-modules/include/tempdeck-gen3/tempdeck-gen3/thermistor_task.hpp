#pragma once

#include "hal/message_queue.hpp"
#include "tempdeck-gen3/messages.hpp"
#include "tempdeck-gen3/tasks.hpp"

namespace thermistor_task {

template <typename Policy>
concept ThermistorPolicy = requires(Policy& p) {
    // A function to get the system time in ms. This may be relative to any
    // time (startup, epoch, etc) so long as the return value measures
    // millisecond increments.
    { p.get_time_ms() } -> std::same_as<uint32_t>;
};

template <template <class> class QueueImpl>
class ThermistorTask {
  public:
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

    // The task reading the thermistor data should run at this frequency
    static constexpr uint32_t THERMISTOR_READ_FREQ_HZ = 10;
    // Number of 1ms ticks for each thermistor read period
    static constexpr uint32_t THERMISTOR_READ_PERIOD_MS =
        (1000 / THERMISTOR_READ_FREQ_HZ);

    explicit ThermistorTask(Aggregator* aggregator)
        : _task_registry(aggregator) {}
    ThermistorTask(const ThermistorTask& other) = delete;
    auto operator=(const ThermistorTask& other) -> ThermistorTask& = delete;
    ThermistorTask(ThermistorTask&& other) noexcept = delete;
    auto operator=(ThermistorTask&& other) noexcept -> ThermistorTask& = delete;
    ~ThermistorTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <ThermistorPolicy Policy>
    auto run_once(Policy& policy) -> void {
        if (!_task_registry) {
            return;
        }
        messages::ThermistorReadings msg = {
            .timestamp = policy.get_time_ms(), .plate = 0, .heatsink = 0};
        static_cast<void>(_task_registry->send(msg));
    }

  private:
    Aggregator* _task_registry;
};

};  // namespace thermistor_task
