/*
 * the primary interface to the thermal plate task
 */
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <variant>

#include "core/pid.hpp"
#include "core/thermistor_conversion.hpp"
#include "hal/message_queue.hpp"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "thermocycler-refresh/thermal_general.hpp"

/* Need a forward declaration for this because of recursive includes */
namespace tasks {
template <template <class> class Queue>
struct Tasks;
};

namespace thermal_plate_task {

template <typename Policy>
concept ThermalPlateExecutionPolicy = requires(Policy& p, PeltierID id,
                                               PeltierDirection direction) {
    // A set_enabled function with inputs of `false` or `true` that
    // sets the enable pin for the peltiers off or on
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.set_enabled(false)};
    // A set_peltier function with inputs to select a peltier,
    // set a power (from 0 to 1.0), and set a direction (heating or
    // cooling)
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    { p.set_peltier(id, 1.0F, direction) } -> std::same_as<bool>;
    // A get_peltier function to return the current direction and
    // power of a peltier.
    { p.get_peltier(id) } -> std::same_as<std::pair<PeltierDirection, double>>;
    // A set_fan function to set the power of the heatsink fan as
    // a percentage from 0 to 1.0
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    { p.set_fan(1.0F) } -> std::same_as<bool>;
};

/** Just used for initialization assignment of error bits.*/
constexpr auto thermistorErrorBit(const ThermistorID id) -> uint16_t {
    if (id > THERM_HEATSINK) {
        throw std::out_of_range("");
    }
    return (1 << id);
}

struct State {
    enum Status {
        IDLE,        /**< Not doing anything.*/
        ERROR,       /**< Experiencing an error*/
        CONTROLLING, /**< Controlling temperature (PID)*/
        PWM_TEST     /**< Testing PWM output (debug command)*/
    };
    Status system_status;
    uint16_t error_bitmap;
    // NOTE - thermistor error bits are defined in the thermistor array
    // initializor. Additional errors are defined assuming the max thermistor
    // error is (1 << 6), for the heat sink
};

// By using a template template parameter here, we allow the code instantiating
// this template to do so as ThermalPlateTask<SomeQueueImpl> rather than
// ThermalPlateTask<SomeQueueImpl<Message>>
using Message = messages::ThermalPlateMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class ThermalPlateTask {
  public:
    using Queue = QueueImpl<Message>;
    static constexpr const uint32_t CONTROL_PERIOD_TICKS = 50;
    static constexpr double THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM = 10.0;
    static constexpr uint16_t ADC_BIT_MAX = 0x5DC0;
    static constexpr uint8_t PLATE_THERM_COUNT = 7;
    // TODO most of these defaults will have to change
    static constexpr double DEFAULT_KI = 0.102;
    static constexpr double DEFAULT_KP = 0.97;
    static constexpr double DEFAULT_KD = 1.901;
    static constexpr double KP_MIN = -200;
    static constexpr double KP_MAX = 200;
    static constexpr double KI_MIN = -200;
    static constexpr double KI_MAX = 200;
    static constexpr double KD_MIN = -200;
    static constexpr double KD_MAX = 200;
    static constexpr double OVERTEMP_LIMIT_C = 105;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    static constexpr const double CONTROL_PERIOD_SECONDS =
        CONTROL_PERIOD_TICKS * 0.001;

    explicit ThermalPlateTask(Queue& q)
        : _message_queue(q),
          _task_registry(nullptr),
          _peltier_left{.id = PELTIER_LEFT,
                        .temp_current = 0.0,
                        .temp_target = 0.0,
                        .pid = PID(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD,
                                   CONTROL_PERIOD_SECONDS, 1.0, -1.0)},
          _peltier_right{.id = PELTIER_RIGHT,
                         .temp_current = 0.0,
                         .temp_target = 0.0,
                         .pid = PID(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD,
                                    CONTROL_PERIOD_SECONDS, 1.0, -1.0)},
          _peltier_center{.id = PELTIER_CENTER,
                          .temp_current = 0.0,
                          .temp_target = 0.0,
                          .pid = PID(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD,
                                     CONTROL_PERIOD_SECONDS, 1.0, -1.0)},
          _thermistors{
              {{.overtemp_limit_c = OVERTEMP_LIMIT_C,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_FRONT_RIGHT_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_FRONT_RIGHT_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_FRONT_RIGHT_OVERTEMP,
                .error_bit = thermistorErrorBit(THERM_FRONT_RIGHT)},
               {.overtemp_limit_c = OVERTEMP_LIMIT_C,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_FRONT_LEFT_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_FRONT_LEFT_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_FRONT_LEFT_OVERTEMP,
                .error_bit = thermistorErrorBit(THERM_FRONT_LEFT)},
               {.overtemp_limit_c = OVERTEMP_LIMIT_C,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_FRONT_CENTER_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_FRONT_CENTER_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_FRONT_CENTER_OVERTEMP,
                .error_bit = thermistorErrorBit(THERM_FRONT_CENTER)},
               {.overtemp_limit_c = OVERTEMP_LIMIT_C,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_BACK_RIGHT_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_BACK_RIGHT_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_BACK_RIGHT_OVERTEMP,
                .error_bit = thermistorErrorBit(THERM_BACK_RIGHT)},
               {.overtemp_limit_c = OVERTEMP_LIMIT_C,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_BACK_LEFT_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_BACK_LEFT_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_BACK_LEFT_OVERTEMP,
                .error_bit = thermistorErrorBit(THERM_BACK_LEFT)},
               {.overtemp_limit_c = OVERTEMP_LIMIT_C,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_BACK_CENTER_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_BACK_CENTER_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_BACK_CENTER_OVERTEMP,
                .error_bit = thermistorErrorBit(THERM_BACK_CENTER)},
               {.overtemp_limit_c = OVERTEMP_LIMIT_C,
                .disconnected_error =
                    errors::ErrorCode::THERMISTOR_HEATSINK_DISCONNECTED,
                .short_error = errors::ErrorCode::THERMISTOR_HEATSINK_SHORT,
                .overtemp_error =
                    errors::ErrorCode::THERMISTOR_HEATSINK_OVERTEMP,
                .error_bit = thermistorErrorBit(THERM_HEATSINK)}}},
          _converter(THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_MAX,
                     false),
          _state{.system_status = State::IDLE, .error_bitmap = 0} {}
    ThermalPlateTask(const ThermalPlateTask& other) = delete;
    auto operator=(const ThermalPlateTask& other) -> ThermalPlateTask& = delete;
    ThermalPlateTask(ThermalPlateTask&& other) noexcept = delete;
    auto operator=(ThermalPlateTask&& other) noexcept
        -> ThermalPlateTask& = delete;
    ~ThermalPlateTask() = default;
    auto get_message_queue() -> Queue& { return _message_queue; }

    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        _task_registry = other_tasks;
    }

    /**
     * run_once() runs one spin of the task. This means it
     * - Waits for a message, either a thermistor update or
     *   some other control message
     * - If there's a message, handles the message
     *   - which may include altering its controller state
     *   - which may include sending a response
     * - Runs its controller
     *
     * The passed-in policy is the hardware interface and must fulfill the
     * ThermalPlateExecutionPolicy concept above.
     * */
    template <typename Policy>
    requires ThermalPlateExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.

        static_cast<void>(_message_queue.recv(&message));
        std::visit(
            [this, &policy](const auto& msg) -> void {
                this->visit_message(msg, policy);
            },
            message);
    }

  private:
    template <typename Policy>
    requires ThermalPlateExecutionPolicy<Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(policy);
        static_cast<void>(_ignore);
    }

    template <typename Policy>
    requires ThermalPlateExecutionPolicy<Policy>
    auto visit_message(const messages::ThermalPlateTempReadComplete& msg,
                       Policy& policy) -> void {
        auto old_error_bitmap = _state.error_bitmap;
        handle_temperature_conversion(msg.front_right,
                                      _thermistors[THERM_FRONT_RIGHT]);
        handle_temperature_conversion(msg.front_left,
                                      _thermistors[THERM_FRONT_LEFT]);
        handle_temperature_conversion(msg.front_center,
                                      _thermistors[THERM_FRONT_CENTER]);
        handle_temperature_conversion(msg.back_right,
                                      _thermistors[THERM_BACK_RIGHT]);
        handle_temperature_conversion(msg.back_left,
                                      _thermistors[THERM_BACK_LEFT]);
        handle_temperature_conversion(msg.back_center,
                                      _thermistors[THERM_BACK_CENTER]);
        handle_temperature_conversion(msg.heat_sink,
                                      _thermistors[THERM_HEATSINK]);

        if (old_error_bitmap != _state.error_bitmap) {
            if (_state.error_bitmap != 0) {
                // We entered an error state. Disable power output.
                _state.system_status = State::ERROR;
                policy.set_enabled(false);
            } else {
                // We went from an error state to no error state... so go idle
                _state.system_status = State::IDLE;
            }
        }

        // TODO update outputs from PID if enabled
    }

    template <typename Policy>
    requires ThermalPlateExecutionPolicy<Policy>
    auto visit_message(const messages::GetPlateTemperatureDebugMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        auto response = messages::GetPlateTemperatureDebugResponse{
            .responding_to_id = msg.id,
            .heat_sink_temp = _thermistors[THERM_HEATSINK].temp_c,
            .front_right_temp = _thermistors[THERM_FRONT_RIGHT].temp_c,
            .front_center_temp = _thermistors[THERM_FRONT_CENTER].temp_c,
            .front_left_temp = _thermistors[THERM_FRONT_LEFT].temp_c,
            .back_right_temp = _thermistors[THERM_BACK_RIGHT].temp_c,
            .back_center_temp = _thermistors[THERM_BACK_CENTER].temp_c,
            .back_left_temp = _thermistors[THERM_BACK_LEFT].temp_c,
            .heat_sink_adc = _thermistors[THERM_HEATSINK].last_adc,
            .front_right_adc = _thermistors[THERM_FRONT_RIGHT].last_adc,
            .front_center_adc = _thermistors[THERM_FRONT_CENTER].last_adc,
            .front_left_adc = _thermistors[THERM_FRONT_LEFT].last_adc,
            .back_right_adc = _thermistors[THERM_BACK_RIGHT].last_adc,
            .back_center_adc = _thermistors[THERM_BACK_CENTER].last_adc,
            .back_left_adc = _thermistors[THERM_BACK_LEFT].last_adc};
        static_cast<void>(_task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <ThermalPlateExecutionPolicy Policy>
    auto visit_message(const messages::SetPeltierDebugMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (_state.system_status == State::ERROR) {
            response.with_error = most_relevant_error();
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(response));
            return;
        }
        if (_state.system_status == State::CONTROLLING) {
            // Send busy error
            response.with_error = errors::ErrorCode::THERMAL_PLATE_BUSY;
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(response));
            return;
        }
        bool ok = true;
        if (msg.selection == PeltierSelection::LEFT ||
            msg.selection == PeltierSelection::ALL) {
            if (!policy.set_peltier(_peltier_left.id, msg.power,
                                    msg.direction)) {
                ok = false;
            }
        }
        if (msg.selection == PeltierSelection::RIGHT ||
            msg.selection == PeltierSelection::ALL) {
            if (!policy.set_peltier(_peltier_right.id, msg.power,
                                    msg.direction)) {
                ok = false;
            }
        }
        if (msg.selection == PeltierSelection::CENTER ||
            msg.selection == PeltierSelection::ALL) {
            if (!policy.set_peltier(_peltier_center.id, msg.power,
                                    msg.direction)) {
                ok = false;
            }
        }
        // Check if we turned off everything
        auto left_pwr = policy.get_peltier(_peltier_left.id);
        auto right_pwr = policy.get_peltier(_peltier_right.id);
        auto center_pwr = policy.get_peltier(_peltier_center.id);
        bool enabled = ((left_pwr.second > 0.0F) || (right_pwr.second > 0.0F) ||
                        (center_pwr.second > 0.0F));
        // If setting a peltier failed somehow, turn everything off
        if (!ok) {
            enabled = false;
        }
        policy.set_enabled(enabled);
        _state.system_status = (enabled) ? State::PWM_TEST : State::IDLE;

        if (!ok) {
            response.with_error = errors::ErrorCode::THERMAL_PELTIER_ERROR;
        }

        static_cast<void>(
            _task_registry->comms->get_message_queue().try_send(response));
    }

    template <ThermalPlateExecutionPolicy Policy>
    auto visit_message(const messages::SetFanManualMessage& msg, Policy& policy)
        -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (_state.system_status == State::ERROR) {
            response.with_error = most_relevant_error();
            static_cast<void>(
                _task_registry->comms->get_message_queue().try_send(response));
            return;
        }
        if (!policy.set_fan(msg.power)) {
            response.with_error = errors::ErrorCode::THERMAL_HEATSINK_FAN_ERROR;
        }

        static_cast<void>(
            _task_registry->comms->get_message_queue().try_send(response));
    }

    auto handle_temperature_conversion(uint16_t conversion_result,
                                       Thermistor& thermistor) -> void {
        auto visitor = [this, &thermistor](const auto value) -> void {
            this->visit_conversion(thermistor, value);
        };

        thermistor.last_adc = conversion_result;
        auto old_error = thermistor.error;
        std::visit(visitor, _converter.convert(conversion_result));
        if (old_error != thermistor.error) {
            if (thermistor.error != errors::ErrorCode::NO_ERROR) {
                _state.error_bitmap |= thermistor.error_bit;
                auto error_message = messages::HostCommsMessage(
                    messages::ErrorMessage{.code = thermistor.error});
                static_cast<void>(
                    _task_registry->comms->get_message_queue().try_send(
                        error_message));
            } else {
                _state.error_bitmap &= ~thermistor.error_bit;
            }
        }
    }

    auto visit_conversion(Thermistor& therm,
                          const thermistor_conversion::Error error) -> void {
        switch (error) {
            case thermistor_conversion::Error::OUT_OF_RANGE_LOW: {
                therm.temp_c = 0;
                therm.error = therm.disconnected_error;
                break;
            }
            case thermistor_conversion::Error::OUT_OF_RANGE_HIGH: {
                therm.temp_c = 0;
                therm.error = therm.short_error;
                break;
            }
        }
    }

    auto visit_conversion(Thermistor& therm, const double temp) -> void {
        if (temp > therm.overtemp_limit_c) {
            therm.error = therm.overtemp_error;
        } else {
            therm.error = errors::ErrorCode::NO_ERROR;
        }
        therm.temp_c = temp;
    }

    [[nodiscard]] auto most_relevant_error() const -> errors::ErrorCode {
        // Sometimes more than one error can occur at the same time; sometimes,
        // that means that one has caused the other. We want to track them
        // separately, but we also sometimes want to respond with just one error
        // condition that sums everything up. This method is used by code that
        // wants the single most relevant code for the current error condition.
        for (auto therm : _thermistors) {
            if ((_state.error_bitmap & therm.error_bit) == therm.error_bit) {
                return therm.error;
            }
        }
        return errors::ErrorCode::NO_ERROR;
    }

    Queue& _message_queue;
    tasks::Tasks<QueueImpl>* _task_registry;
    Peltier _peltier_left;
    Peltier _peltier_right;
    Peltier _peltier_center;
    std::array<Thermistor, PLATE_THERM_COUNT> _thermistors;
    thermistor_conversion::Conversion<lookups::KS103J2G> _converter;
    State _state;
};

}  // namespace thermal_plate_task
