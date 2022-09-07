#pragma once

#include "core/ads1115.hpp"
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
    // A function to sleep the task for a configurable number of milliseconds.
    // Used to provide a delay between thermistor read retries.
    { p.sleep_ms(1) } -> std::same_as<void>;
    // A function to read the last Peltier Feedback reading
    { p.get_imeas_adc_reading() } -> std::same_as<uint32_t>;
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
        auto adc = ADS1115::ADC(policy);

        if (!adc.initialized()) {
            adc.initialize();
        }

        messages::ThermistorReadings msg = {
            .timestamp = policy.get_time_ms(), .plate = 0, .heatsink = 0};

        msg.plate = read_pin(adc, 0, policy);
        msg.heatsink = read_pin(adc, 1, policy);
        msg.imeas = policy.get_imeas_adc_reading();
        static_cast<void>(_task_registry->send(msg));
    }

  private:
    template <ThermistorPolicy Policy>
    auto read_pin(ADS1115::ADC<Policy>& adc, uint16_t pin, Policy& policy)
        -> uint16_t {
        static constexpr uint8_t MAX_TRIES = 5;
        static constexpr uint32_t RETRY_DELAY = 5;
        uint8_t tries = 0;
        auto result = typename ADS1115::ADC<Policy>::ReadVal();

        while (true) {
            result = adc.read(pin);
            if (std::holds_alternative<uint16_t>(result)) {
                return std::get<uint16_t>(result);
            }
            if (++tries < MAX_TRIES) {
                // Short delay for reliability
                policy.sleep_ms(RETRY_DELAY);
            } else {
                // Retries expired
                return static_cast<uint16_t>(std::get<ADS1115::Error>(result));
            }
        }
    }

    Aggregator* _task_registry;
};

};  // namespace thermistor_task
