#pragma once

#include <optional>

#include "core/thermistor_conversion.hpp"
#include "hal/message_queue.hpp"
#include "ot_utils/core/pid.hpp"
#include "tempdeck-gen3/eeprom.hpp"
#include "tempdeck-gen3/messages.hpp"
#include "tempdeck-gen3/tasks.hpp"
#include "thermistor_lookups.hpp"

namespace thermal_task {

template <typename Policy>
concept ThermalPolicy = requires(Policy& p) {
    { p.enable_peltier() } -> std::same_as<void>;
    { p.disable_peltier() } -> std::same_as<void>;
    { p.set_peltier_heat_power(1.0) } -> std::same_as<bool>;
    { p.set_peltier_cool_power(1.0) } -> std::same_as<bool>;
    { p.set_fan_power(1.0) } -> std::same_as<bool>;
    { p.get_fan_rpm() } -> std::same_as<double>;
}
&&at24c0xc::AT24C0xC_Policy<Policy>;

using Message = messages::ThermalMessage;

struct ThermalReadings {
    uint32_t plate_adc = 0;
    uint32_t heatsink_adc = 0;
    uint32_t peltier_current_adc = 0;
    std::optional<double> heatsink_temp = 0.0F;
    std::optional<double> plate_temp = 0.0F;
    double peltier_current_milliamps = 0.0F;

    uint32_t last_tick = 0;
};

struct Fan {
    bool manual = false;
    double power = 0.0F;
};

struct Peltier {
    bool manual = false;
    bool target_set = false;
    double power = 0.0F;
    double target = 0.0F;
};

// Provides constants & conversions for the internal ADC
struct PeltierReadback {
    // Internal ADC max value is 12 bits = 0xFFF = 4095
    static constexpr double MAX_ADC_COUNTS = 4096;
    // Internal ADC is scaled to 3.3v max
    static constexpr double MAX_ADC_VOLTAGE = 3.3;
    // Amps per volt based on schematic
    static constexpr double MILLIAMPS_PER_VOLT = 3773;
    // Constant offset C for a y = mx + b regression
    static constexpr double MILLIAMP_OFFSET = -6225;
    // Final conversion factor between adc and current
    static constexpr double MILLIAMPS_PER_COUNT =
        ((MAX_ADC_VOLTAGE * MILLIAMPS_PER_VOLT) / MAX_ADC_COUNTS);

    static auto adc_to_milliamps(uint32_t adc) -> double {
        return (static_cast<double>(adc) * MILLIAMPS_PER_COUNT) +
               MILLIAMP_OFFSET;
    }

    static auto milliamps_to_adc(double milliamps) -> uint32_t {
        return static_cast<uint32_t>((milliamps - MILLIAMP_OFFSET) /
                                     MILLIAMPS_PER_COUNT);
    }
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class ThermalTask {
  public:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

    using Celsius = double;

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

    // The threshold at which the fan is turned on to cool the heatsink
    // during idle periods.
    static constexpr Celsius HEATSINK_IDLE_THRESHOLD = 30.0F;

    static constexpr Celsius COOL_THRESHOLD = 20.0;
    static constexpr Celsius HOT_THRESHOLD = 30.0;

    static constexpr Celsius STABILIZING_THRESHOLD = 0.5;

    static constexpr double FAN_POWER_LOW = 0.2;
    static constexpr double FAN_POWER_MEDIUM = 0.75;
    static constexpr double FAN_POWER_MAX = 1.0;

    static constexpr double PELTIER_KP_HEATING_DEFAULT = 0.141637;
    static constexpr double PELTIER_KI_HEATING_DEFAULT = 0.005339;
    static constexpr double PELTIER_KD_DEFAULT = 0.0F;
    static constexpr double PELTIER_KP_COOLING_DEFAULT = 0.483411;
    static constexpr double PELTIER_KI_COOLING_DEFAULT = 0.023914;

    static constexpr double PELTIER_K_MAX = 200.0F;
    static constexpr double PELTIER_K_MIN = -200.0F;
    static constexpr double PELTIER_WINDUP_LIMIT = 1.0F;

    static constexpr double MILLISECONDS_TO_SECONDS = 0.001F;

    static constexpr size_t EEPROM_PAGES = 32;
    static constexpr uint8_t EEPROM_ADDRESS = 0b1010010;

    static constexpr const double OFFSET_DEFAULT_CONST_A = 0.0F;
    static constexpr const double OFFSET_DEFAULT_CONST_B = 0.0F;
    static constexpr const double OFFSET_DEFAULT_CONST_C = 0.0F;

    explicit ThermalTask(Queue& q, Aggregator* aggregator)
        : _message_queue(q),
          _task_registry(aggregator),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _readings(),
          _converter(THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_MAX,
                     false),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _fan(),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _peltier(),
          _pid(PELTIER_KP_HEATING_DEFAULT, PELTIER_KI_HEATING_DEFAULT,
               PELTIER_KD_DEFAULT, 1.0F, PELTIER_WINDUP_LIMIT,
               -PELTIER_WINDUP_LIMIT),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          _eeprom(),
          _offset_constants{.a = OFFSET_DEFAULT_CONST_A,
                            .b = OFFSET_DEFAULT_CONST_B,
                            .c = OFFSET_DEFAULT_CONST_C} {}
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
        // If the EEPROM data hasn't been read, read it before doing
        // anything else.
        if (!_eeprom.initialized()) {
            _offset_constants =
                _eeprom.get_offset_constants(_offset_constants, policy);
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

    [[nodiscard]] auto get_fan() const -> Fan { return _fan; }

    [[nodiscard]] auto get_peltier() const -> Peltier { return _peltier; }

    [[nodiscard]] auto get_pid() const -> ot_utils::pid::PID { return _pid; }

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

        auto tick_difference = message.timestamp - _readings.last_tick;

        _readings.heatsink_adc = message.heatsink;
        _readings.plate_adc = message.plate;
        _readings.peltier_current_adc = message.imeas;
        _readings.last_tick = message.timestamp;
        // Reading conversion

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

        _readings.peltier_current_milliamps =
            PeltierReadback::adc_to_milliamps(message.imeas);

        // Update thermal control

        update_thermal_control(policy,
                               tick_difference * MILLISECONDS_TO_SECONDS);
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::DeactivateAllMessage& message,
                       Policy& policy) -> void {
        _fan.manual = false;
        policy.set_fan_power(0);

        _peltier.manual = false;
        _peltier.target_set = false;
        policy.disable_peltier();

        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::GetTempDebugMessage& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);

        auto response = messages::GetTempDebugResponse{
            .responding_to_id = message.id,
            .plate_temp = 0,
            .heatsink_temp = 0,
            .plate_adc = static_cast<uint16_t>(_readings.plate_adc),
            .heatsink_adc = static_cast<uint16_t>(_readings.heatsink_adc)};
        if (_readings.plate_temp.has_value()) {
            response.plate_temp =
                static_cast<float>(_readings.plate_temp.value());
        }
        if (_readings.heatsink_temp.has_value()) {
            response.heatsink_temp =
                static_cast<float>(_readings.heatsink_temp.value());
        }
        static_cast<void>(_task_registry->send(response));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::SetTemperatureMessage& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);

        _peltier.manual = false;
        _peltier.target_set = true;
        _peltier.target = message.target;
        if (_readings.plate_temp.value() < _peltier.target) {
            _pid = ot_utils::pid::PID(
                PELTIER_KP_HEATING_DEFAULT, PELTIER_KI_HEATING_DEFAULT,
                PELTIER_KD_DEFAULT, 1.0, PELTIER_WINDUP_LIMIT,
                -PELTIER_WINDUP_LIMIT);
        } else {
            _pid = ot_utils::pid::PID(
                PELTIER_KP_COOLING_DEFAULT, PELTIER_KI_COOLING_DEFAULT,
                PELTIER_KD_DEFAULT, 1.0, PELTIER_WINDUP_LIMIT,
                -PELTIER_WINDUP_LIMIT);
        }
        _pid.reset();

        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::SetPeltierDebugMessage& message,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        if (_peltier.target_set) {
            // If the thermal task is busy with a target, don't override that
            response.with_error = errors::ErrorCode::THERMAL_PELTIER_BUSY;
        } else {
            if (std::abs(message.power) > 1.0F) {
                response.with_error =
                    errors::ErrorCode::THERMAL_PELTIER_POWER_ERROR;
            } else if (message.power != 0.0F) {
                policy.enable_peltier();
                bool ok = false;
                if (message.power > 0.0F) {
                    ok = policy.set_peltier_heat_power(message.power);
                } else {
                    ok = policy.set_peltier_cool_power(std::abs(message.power));
                }
                if (!ok) {
                    response.with_error =
                        errors::ErrorCode::THERMAL_PELTIER_ERROR;
                    policy.disable_peltier();
                    _peltier.power = 0.0F;
                } else {
                    _peltier.manual = true;
                    _peltier.power = message.power;
                }
            } else {
                policy.disable_peltier();
                _peltier.manual = false;
            }
        }

        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::SetFanManualMessage& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        _fan.manual = true;
        _fan.power = std::clamp(message.power, double(0.0F), double(1.0F));

        policy.set_fan_power(_fan.power);

        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::SetFanAutomaticMessage& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        _fan.manual = false;

        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::SetPIDConstantsMessage& message,
                       Policy& policy) -> void {
        static_cast<void>(policy);

        auto p = std::clamp(message.p, PELTIER_K_MIN, PELTIER_K_MAX);
        auto i = std::clamp(message.i, PELTIER_K_MIN, PELTIER_K_MAX);
        auto d = std::clamp(message.d, PELTIER_K_MIN, PELTIER_K_MAX);

        _pid = ot_utils::pid::PID(p, i, d, 1.0, PELTIER_WINDUP_LIMIT,
                                  -PELTIER_WINDUP_LIMIT);

        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::SetOffsetConstantsMessage& message,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = message.id};

        auto constants = _offset_constants;
        if (message.a.has_value()) {
            constants.a = message.a.value();
        }
        if (message.b.has_value()) {
            constants.b = message.b.value();
        }
        if (message.c.has_value()) {
            constants.c = message.c.value();
        }

        auto ret = _eeprom.write_offset_constants(constants, policy);
        if (ret) {
            // Succesful, so overwrite the task's constants
            _offset_constants = constants;
        } else {
            response.with_error = errors::ErrorCode::SYSTEM_EEPROM_ERROR;
        }

        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::GetOffsetConstantsMessage& message,
                       Policy& policy) -> void {
        _offset_constants =
            _eeprom.get_offset_constants(_offset_constants, policy);

        auto response =
            messages::GetOffsetConstantsResponse{.responding_to_id = message.id,
                                                 .a = _offset_constants.a,
                                                 .b = _offset_constants.b,
                                                 .c = _offset_constants.c};

        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    template <ThermalPolicy Policy>
    auto visit_message(const messages::GetThermalPowerDebugMessage& message,
                       Policy& policy) -> void {
        std::ignore = policy;
        auto response = messages::GetThermalPowerDebugResponse{
            .responding_to_id = message.id,
            .peltier_current = _readings.peltier_current_milliamps,
            .fan_rpm = 0,
            .peltier_pwm = _peltier.power,
            .fan_pwm = _fan.power};

        if (!_peltier.target_set && !_peltier.manual) {
            response.peltier_pwm = 0.0F;
        }
        response.fan_rpm = policy.get_fan_rpm();
        static_cast<void>(
            _task_registry->send_to_address(response, Queues::HostAddress));
    }

    /**
     * @brief Updates control of the peltier and fan based off of the current
     * state of the system.
     *
     * @param[in] policy The hardware control policy
     * @param[in] sampletime The number of seconds since the last temp reading
     */
    template <ThermalPolicy Policy>
    auto update_thermal_control(Policy& policy, double sampletime) -> void {
        if (_peltier.target_set) {
            if (!_readings.plate_temp.has_value()) {
                _peltier.target_set = false;
                policy.disable_peltier();
            } else {
                auto power = _pid.compute(
                    _peltier.target - _readings.plate_temp.value(), sampletime);
                _peltier.power = std::clamp(power, -1.0, 1.0);
                policy.enable_peltier();
                bool ret = false;
                if (_peltier.power >= 0.0F) {
                    ret = policy.set_peltier_heat_power(_peltier.power);
                } else {
                    ret =
                        policy.set_peltier_cool_power(std::abs(_peltier.power));
                }
                if (!ret) {
                    _peltier.target_set = false;
                    policy.disable_peltier();
                }
            }
        }
        if (!_fan.manual) {
            if (_peltier.target_set) {
                // We know for a fact that the plate_temp exists because
                // target_set hasn't been cleared
                if (_readings.plate_temp.value() >
                    _peltier.target + STABILIZING_THRESHOLD) {
                    _fan.power = FAN_POWER_MAX;
                } else {
                    _fan.power = FAN_POWER_MEDIUM;
                }
            } else /* !_peltier.target_set */ {
                if (_readings.heatsink_temp.has_value() &&
                    _readings.heatsink_temp.value() < HEATSINK_IDLE_THRESHOLD) {
                    _fan.power = 0;
                } else {
                    _fan.power = FAN_POWER_LOW;
                }
            }
            policy.set_fan_power(_fan.power);
        }
    }

    Queue& _message_queue;
    Aggregator* _task_registry;
    ThermalReadings _readings;
    thermistor_conversion::Conversion<lookups::KS103J2G> _converter;
    Fan _fan;
    Peltier _peltier;
    ot_utils::pid::PID _pid;
    eeprom::Eeprom<EEPROM_PAGES, EEPROM_ADDRESS> _eeprom;
    eeprom::OffsetConstants _offset_constants;
};

};  // namespace thermal_task
