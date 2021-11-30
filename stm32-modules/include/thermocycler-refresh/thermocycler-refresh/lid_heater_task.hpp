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
#include "thermistor_lookups.hpp"
#include "thermocycler-refresh/errors.hpp"
#include "thermocycler-refresh/messages.hpp"
#include "thermocycler-refresh/tasks.hpp"
#include "thermocycler-refresh/thermal_general.hpp"

/* Need a forward declaration for this because of recursive includes */
namespace tasks {
template <template <class> class Queue>
struct Tasks;
};

namespace lid_heater_task {

template <typename Policy>
concept LidHeaterExecutionPolicy = requires(Policy& p, const Policy& cp) {
    // A set_enabled function with inputs of `false` or `true` that
    // sets the enable pin for the peltiers off or on
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.set_enabled(false)};
};

struct State {
    enum Status {
        IDLE,
        ERROR,
        CONTROLLING,
    };
    Status system_status;
    uint16_t error_bitmap;
    static constexpr uint16_t LID_THERMISTOR_ERROR = (1 << 0);
    static constexpr uint16_t HEATER_POWER_ERROR = (1 << 1);
};

// By using a template template parameter here, we allow the code instantiating
// this template to do so as LidHeaterTask<SomeQueueImpl> rather than
// LidHeaterTask<SomeQueueImpl<Message>>
using Message = messages::LidHeaterMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class LidHeaterTask {
  public:
    using Queue = QueueImpl<Message>;
    static constexpr const uint32_t CONTROL_PERIOD_TICKS = 100;
    static constexpr double THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM = 10.0;
    static constexpr uint16_t ADC_BIT_MAX = 0x5DC0;
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
    static constexpr double OVERTEMP_LIMIT_C = 95;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    static constexpr const double CONTROL_PERIOD_SECONDS =
        CONTROL_PERIOD_TICKS * 0.001;

    explicit LidHeaterTask(Queue& q)
        : _message_queue(q),
          _task_registry(nullptr),
          _thermistor{
              .overtemp_limit_c = OVERTEMP_LIMIT_C,
              .disconnected_error =
                  errors::ErrorCode::THERMISTOR_LID_DISCONNECTED,
              .short_error = errors::ErrorCode::THERMISTOR_LID_SHORT,
              .overtemp_error = errors::ErrorCode::THERMISTOR_LID_OVERTEMP,
              .error_bit = State::LID_THERMISTOR_ERROR},
          _converter(THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_MAX,
                     false),
          _state{.system_status = State::IDLE, .error_bitmap = 0},
          _pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, CONTROL_PERIOD_SECONDS, 1.0,
               -1.0) {}
    LidHeaterTask(const LidHeaterTask& other) = delete;
    auto operator=(const LidHeaterTask& other) -> LidHeaterTask& = delete;
    LidHeaterTask(LidHeaterTask&& other) noexcept = delete;
    auto operator=(LidHeaterTask&& other) noexcept -> LidHeaterTask& = delete;
    ~LidHeaterTask() = default;
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
     * LidHeaterExecutionPolicy concept above.
     * */
    template <typename Policy>
    requires LidHeaterExecutionPolicy<Policy>
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
    requires LidHeaterExecutionPolicy<Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(policy);
        static_cast<void>(_ignore);
    }

    template <typename Policy>
    requires LidHeaterExecutionPolicy<Policy>
    auto visit_message(const messages::LidTempReadComplete& msg, Policy& policy)
        -> void {
        static_cast<void>(policy);

        handle_temperature_conversion(msg.lid_temp, _thermistor);
    }

    template <typename Policy>
    requires LidHeaterExecutionPolicy<Policy>
    auto visit_message(const messages::GetLidTemperatureDebugMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        auto response = messages::GetLidTemperatureDebugResponse{
            .responding_to_id = msg.id,
            .lid_temp = _thermistor.temp_c,
            .lid_adc = _thermistor.last_adc};
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

    Queue& _message_queue;
    tasks::Tasks<QueueImpl>* _task_registry;
    Thermistor _thermistor;
    thermistor_conversion::Conversion<lookups::KS103J2G> _converter;
    State _state;
    PID _pid;
};

}  // namespace lid_heater_task
