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
concept ThermalPlateExecutionPolicy = requires(Policy& p, const Policy& cp) {
    // A set_enabled function with inputs of `false` or `true` that
    // sets the enable pin for the peltiers off or on
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.set_enabled(false)};
};

/** Just used for initialization assignment of error bits.*/
constexpr uint16_t thermistorErrorBit(const ThermistorID id) {
    if (id > THERM_HEATSINK) {
        throw std::out_of_range("");
    }
    return (1 << id);
}

struct State {
    enum Status {
        IDLE,
        ERROR,
        CONTROLLING,
    };
    Status system_status;
    uint16_t error_bitmap;
    // NOTE - these values are defined assuming the max thermistor error is
    // (1 << 6), for the heat sink
    static constexpr uint16_t OVERTEMP_PLATE_ERROR = (1 << 7);
    static constexpr uint16_t OVERTEMP_HEATSINK_ERROR = (1 << 7);
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
                        .enabled = false,
                        .temp_current = 0.0,
                        .temp_target = 0.0},
          _peltier_right{.id = PELTIER_RIGHT,
                         .enabled = false,
                         .temp_current = 0.0,
                         .temp_target = 0.0},
          _peltier_center{.id = PELTIER_CENTER,
                          .enabled = false,
                          .temp_current = 0.0,
                          .temp_target = 0.0},
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
          _state{.system_status = State::IDLE, .error_bitmap = 0},
          _plate_pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, CONTROL_PERIOD_SECONDS,
                     1.0, -1.0) {}
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
    }

    template <typename Policy>
    requires ThermalPlateExecutionPolicy<Policy>
    auto visit_message(const messages::GetPlateTemperatureDebugMessage& msg,
                       Policy& policy) -> void {
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

    Queue& _message_queue;
    tasks::Tasks<QueueImpl>* _task_registry;
    Peltier _peltier_left;
    Peltier _peltier_right;
    Peltier _peltier_center;
    std::array<Thermistor, PLATE_THERM_COUNT> _thermistors;
    thermistor_conversion::Conversion<lookups::KS103J2G> _converter;
    State _state;
    PID _plate_pid;
};

}  // namespace thermal_plate_task
