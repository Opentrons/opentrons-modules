#pragma once

#include "core/thermistor_conversion.hpp"
#include "hal/message_queue.hpp"
#include "tempdeck-gen3/messages.hpp"
#include "tempdeck-gen3/tasks.hpp"
#include "thermistor_lookups.hpp"

namespace thermal_task {

template <typename Policy>
concept ThermalPolicy = requires(Policy& p) {
    {p.enable_peltier()} -> std::same_as<void>;
    {p.disable_peltier()} -> std::same_as<void>;
    {p.set_peltier_heat_power(1.0)} -> std::same_as<bool>;
    {p.set_peltier_cool_power(1.0)} -> std::same_as<bool>;
};

using Message = messages::ThermalMessage;

struct ThermalReadings {
    uint32_t plate_adc = 0;
    uint32_t heatsink_adc = 0;
    double heatsink_temp = 0.0F;
    double plate_temp = 0.0F;

    uint32_t last_tick = 0;
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class ThermalTask {
  public:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

    // Bias resistance, aka the pullup resistance in the thermistor voltage
    // divider circuit.
    static constexpr double THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM = 45.3;
    // Default VREF for the ADC driver
    static constexpr double ADC_VREF = 2.048;
    // The circuit is configured such that 1.5v is the max voltage from the
    // thermistor.
    static constexpr double ADC_MAX_V = 1.5;
    // ADC results are signed 16-bit integers
    static constexpr uint16_t ADC_BIT_MAX = static_cast<uint16_t>(
        (ADC_MAX_V * static_cast<double>(0x7FFF)) / ADC_VREF);

    explicit ThermalTask(Queue& q, Aggregator* aggregator)
        : _message_queue(q),
          _task_registry(aggregator),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _readings(),
          _converter(THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_MAX,
                     false) {}
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

        auto res = _converter.convert(_readings.plate_adc);
        if (std::holds_alternative<double>(res)) {
            _readings.plate_temp = std::get<double>(res);
        } else {
            _readings.plate_temp = 0.0F;
        }
        res = _converter.convert(_readings.heatsink_adc);
        if (std::holds_alternative<double>(res)) {
            _readings.heatsink_temp = std::get<double>(res);
        } else {
            _readings.heatsink_temp = 0.0F;
        }
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::GetTempDebugMessage& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);

        auto response = messages::GetTempDebugResponse{
            .responding_to_id = message.id,
            .plate_temp = static_cast<float>(_readings.plate_temp),
            .heatsink_temp = static_cast<float>(_readings.heatsink_temp),
            .plate_adc = static_cast<uint16_t>(_readings.plate_adc),
            .heatsink_adc = static_cast<uint16_t>(_readings.heatsink_adc)};
        static_cast<void>(_task_registry->send(response));
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    ThermalReadings _readings;
    thermistor_conversion::Conversion<lookups::NXFT15XV103FA2B030> _converter;
};

};  // namespace thermal_task
