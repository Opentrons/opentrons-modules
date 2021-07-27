/*
 * the primary interface to the heater task
 */
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/pid.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater-shaker/thermistor_conversion.hpp"

/* Need a forward declaration for this because of recursive includes */
namespace tasks {
template <template <class> class Queue>
struct Tasks;
};

namespace heater_task {

template <typename Policy>
concept HeaterExecutionPolicy = requires(Policy& p, const Policy& cp) {
    // Check if the hardware is ready (true) or if some errors is preventing
    // power flowing to the heater pad drivers
    { cp.power_good() } -> std::same_as<bool>;
    // Attempt to reset the heater error latch and check if it worked (true)
    // or if the error condition is still present (false)
    { p.try_reset_power_good() } -> std::same_as<bool>;

    // A set_power_output function with inputs between 0 and 1 sets the
    // relative output of the heater pad
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    {p.set_power_output(0.5)};
    // disable_power_output should fully turn off the driver (set_power_output
    // will usually turn it on at least a little bit)
    {p.disable_power_output()};
};

struct State {
    enum Status {
        IDLE,
        ERROR,
        CONTROLLING,
        POWER_TEST,
    };
    Status system_status;
    uint8_t error_bitmap;
    static constexpr uint8_t PAD_A_SENSE_ERROR = (1 << 0);
    static constexpr uint8_t PAD_B_SENSE_ERROR = (1 << 1);
    static constexpr uint8_t PAD_SENSE_ERROR =
        PAD_A_SENSE_ERROR | PAD_B_SENSE_ERROR;
    static constexpr uint8_t BOARD_SENSE_ERROR = (1 << 2);
    static constexpr uint8_t SENSE_ERROR = PAD_SENSE_ERROR | BOARD_SENSE_ERROR;
    static constexpr uint8_t POWER_GOOD_ERROR = (1 << 3);
};

struct TemperatureSensor {
    // The last converted temperature (0 if it was not valid)
    double temp_c = 0;
    // The last ADC conversion result
    uint16_t last_adc = 0;
    // The current error
    errors::ErrorCode error = errors::ErrorCode::NO_ERROR;
    // These static values should be set when this struct is constructed to
    // provide errors specific to a sensor
    const errors::ErrorCode disconnected_error;
    const errors::ErrorCode short_error;
    const errors::ErrorCode overtemp_error;
    const double overtemp_limit_c;
    const thermistor_conversion::Conversion conversion;
    const uint8_t error_bit;
};

// By using a template template parameter here, we allow the code instantiating
// this template to do so as HeaterTask<SomeQueueImpl> rather than
// HeaterTask<SomeQueueImpl<Message>>
using Message = messages::HeaterMessage;
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class HeaterTask {
  public:
    using Queue = QueueImpl<Message>;
    static constexpr const uint32_t CONTROL_PERIOD_TICKS = 100;
    static constexpr double THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM = 44.2;
    static constexpr uint8_t ADC_BIT_DEPTH = 12;
    static constexpr double HEATER_PAD_OVERTEMP_SAFETY_LIMIT_C = 100;
    static constexpr double BOARD_OVERTEMP_SAFETY_LIMIT_C = 60;
    static constexpr double DEFAULT_KI = 0.102;
    static constexpr double DEFAULT_KP = 0.97;
    static constexpr double DEFAULT_KD = 1.901;
    static constexpr double MAX_CONTROLLABLE_TEMPERATURE = 95.0;
    static constexpr double KP_MIN = -200;
    static constexpr double KP_MAX = 200;
    static constexpr double KI_MIN = -200;
    static constexpr double KI_MAX = 200;
    static constexpr double KD_MIN = -200;
    static constexpr double KD_MAX = 200;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    static constexpr double CONTROL_PERIOD_S =
        static_cast<uint32_t>(CONTROL_PERIOD_TICKS) * 0.001;
    explicit HeaterTask(Queue& q)
        : message_queue(q),
          task_registry(nullptr),
          pad_a{
              .disconnected_error =
                  errors::ErrorCode::HEATER_THERMISTOR_A_DISCONNECTED,
              .short_error = errors::ErrorCode::HEATER_THERMISTOR_A_SHORT,
              .overtemp_error = errors::ErrorCode::HEATER_THERMISTOR_A_OVERTEMP,
              .overtemp_limit_c = HEATER_PAD_OVERTEMP_SAFETY_LIMIT_C,
              .conversion = thermistor_conversion::Conversion(
                  thermistor_conversion::ThermistorType::NTCG104ED104DTDSX,
                  THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_DEPTH),
              .error_bit = State::PAD_A_SENSE_ERROR},
          pad_b{
              .disconnected_error =
                  errors::ErrorCode::HEATER_THERMISTOR_B_DISCONNECTED,
              .short_error = errors::ErrorCode::HEATER_THERMISTOR_B_SHORT,
              .overtemp_error = errors::ErrorCode::HEATER_THERMISTOR_B_OVERTEMP,
              .overtemp_limit_c = HEATER_PAD_OVERTEMP_SAFETY_LIMIT_C,
              .conversion = thermistor_conversion::Conversion(
                  thermistor_conversion::ThermistorType::NTCG104ED104DTDSX,
                  THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_DEPTH),
              .error_bit = State::PAD_B_SENSE_ERROR,
          },
          board{.disconnected_error =
                    errors::ErrorCode::HEATER_THERMISTOR_BOARD_DISCONNECTED,
                .short_error = errors::ErrorCode::HEATER_THERMISTOR_BOARD_SHORT,
                .overtemp_error =
                    errors::ErrorCode::HEATER_THERMISTOR_BOARD_OVERTEMP,
                .overtemp_limit_c = BOARD_OVERTEMP_SAFETY_LIMIT_C,
                .conversion = thermistor_conversion::Conversion(
                    thermistor_conversion::ThermistorType::NTCG104ED104DTDSX,
                    THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_DEPTH),
                .error_bit = State::BOARD_SENSE_ERROR},
          state{.system_status = State::IDLE, .error_bitmap = 0},
          pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, CONTROL_PERIOD_S, 1.0, -1.0),
          setpoint(0) {}
    HeaterTask(const HeaterTask& other) = delete;
    auto operator=(const HeaterTask& other) -> HeaterTask& = delete;
    HeaterTask(HeaterTask&& other) noexcept = delete;
    auto operator=(HeaterTask&& other) noexcept -> HeaterTask& = delete;
    ~HeaterTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    // Please don't use this for cross-thread communication it's primarily
    // there for the simulator
    [[nodiscard]] auto get_setpoint() const -> double { return setpoint; }

    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

    /**
     * run_once() runs one spin of the task. This means it
     * - Waits for a message, or for a timeout so it can run its controller
     * - If there's a message, handles the message
     *   - which may include altering its controller state
     *   - which may include sending a response
     * - Runs its controller
     *
     * The passed-in policy is the hardware interface and must fulfill the
     * HeaterExecutionPolicy concept above.
     * */
    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.

        static_cast<void>(message_queue.recv(&message));
        std::visit(
            [this, &policy](const auto& msg) -> void {
                this->visit_message(msg, policy);
            },
            message);
    }

    [[nodiscard]] auto get_pid() const -> const PID& { return pid; }

  private:
    template <typename Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(policy);
        static_cast<void>(_ignore);
    }

    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto visit_message(const messages::SetTemperatureMessage& msg,
                       Policy& policy) -> void {
        // While in error state, we will refuse to set temperatures
        // But we can try and disarm the latch if that's the only problem
        try_latch_disarm(policy);
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (state.system_status == State::ERROR) {
            setpoint = 0;
            response.with_error = most_relevant_error();
        } else {
            setpoint = msg.target_temperature;
            pid.arm_integrator_reset(setpoint - pad_temperature());
            state.system_status = State::CONTROLLING;
        }
        if (msg.from_system) {
            static_cast<void>(
                task_registry->system->get_message_queue().try_send(
                    messages::SystemMessage(response)));
        } else {
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto visit_message(const messages::GetTemperatureMessage& msg,
                       Policy& policy) -> void {
        static_cast<void>(policy);
        errors::ErrorCode code = pad_a.error != errors::ErrorCode::NO_ERROR
                                     ? pad_a.error
                                     : pad_b.error;
        auto response = messages::GetTemperatureResponse{
            .responding_to_id = msg.id,
            .current_temperature = pad_temperature(),
            .setpoint_temperature = setpoint,
            .with_error = code};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto visit_message(const messages::GetTemperatureDebugMessage& msg,
                       Policy& policy) -> void {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        auto response = messages::GetTemperatureDebugResponse{
            .responding_to_id = msg.id,
            .pad_a_temperature = pad_a.temp_c,
            .pad_b_temperature = pad_b.temp_c,
            .board_temperature = board.temp_c,
            .pad_a_adc = pad_a.last_adc,
            .pad_b_adc = pad_b.last_adc,
            .board_adc = board.last_adc,
            .power_good = policy.power_good()};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto visit_message(const messages::SetPIDConstantsMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if ((msg.kp < KP_MIN) || (msg.kp > KP_MAX) || (msg.ki < KI_MIN) ||
            (msg.ki > KI_MAX) || (msg.kd < KD_MIN) || (msg.kd > KD_MAX)) {
            response.with_error =
                errors::ErrorCode::HEATER_CONSTANT_OUT_OF_RANGE;
        } else {
            policy.disable_power_output();
            pid = PID(msg.kp, msg.ki, msg.kd, CONTROL_PERIOD_S, 1.0, -1.0);
        }
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto visit_message(const messages::TemperatureConversionComplete& msg,
                       Policy& policy) -> void {
        auto old_error_bitmap = state.error_bitmap;
        if (!policy.power_good()) {
            state.error_bitmap |= State::POWER_GOOD_ERROR;
        }
        handle_temperature_conversion(msg.pad_a, pad_a);
        handle_temperature_conversion(msg.pad_b, pad_b);
        handle_temperature_conversion(msg.board, board);
        // The error handling wants to accomplish the following:
        // - Only run if there were any changes in the error state for
        //   the sensors or the heater pad power driver
        // - If that change is that the detailed error responses from
        //   the sensors are now gone, try and reset the power driver
        // - If that fails, inform upstream
        // - If the change was that the error latch fired even though it doesn't
        //   seem like it should have, send that error
        // - In any case, make sure the overall system state is correct

        auto changes = old_error_bitmap ^ state.error_bitmap;
        if ((changes & State::PAD_SENSE_ERROR) != 0) {
            if ((state.error_bitmap & State::PAD_SENSE_ERROR) == 0) {
                if (policy.try_reset_power_good()) {
                    state.error_bitmap &= ~State::POWER_GOOD_ERROR;
                    if (state.error_bitmap == 0) {
                        state.system_status = State::IDLE;
                    }
                } else {
                    auto error_message =
                        messages::HostCommsMessage(messages::ErrorMessage{
                            .code = errors::ErrorCode::
                                HEATER_HARDWARE_ERROR_LATCH});
                    static_cast<void>(
                        task_registry->comms->get_message_queue().try_send(
                            error_message));
                    state.system_status = State::ERROR;
                    setpoint = 0;
                }
            } else {
                state.system_status = State::ERROR;
                setpoint = 0;
            }
        } else if ((changes & State::POWER_GOOD_ERROR) != 0) {
            auto error_message =
                messages::HostCommsMessage(messages::ErrorMessage{
                    .code = errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH});
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    error_message));
            state.system_status = State::ERROR;
            setpoint = 0;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        if (state.system_status == State::CONTROLLING) {
            policy.set_power_output(pid.compute(setpoint - pad_temperature()));
        } else if (state.system_status != State::POWER_TEST) {
            policy.disable_power_output();
        }
    }

    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto visit_message(const messages::SetPowerTestMessage& msg, Policy& policy)
        -> void {
        try_latch_disarm(policy);
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (state.system_status == State::ERROR) {
            response.with_error = most_relevant_error();
        } else {
            double power = std::clamp(msg.power, 0.0, 1.0);
            if (power == 0.0) {
                policy.disable_power_output();
            } else {
                policy.set_power_output(power);
            }
            setpoint = power;
            state.system_status = State::POWER_TEST;
        }
        static_cast<void>(
            task_registry->comms->get_message_queue().try_send(response));
    }

    template <typename Policy>
    requires HeaterExecutionPolicy<Policy>
    auto try_latch_disarm(Policy& policy) -> void {
        if (!policy.power_good() &&
            ((state.error_bitmap & State::PAD_SENSE_ERROR) == 0)) {
            if (policy.try_reset_power_good()) {
                state.error_bitmap &= ~State::POWER_GOOD_ERROR;
                state.system_status = State::IDLE;
            } else {
                state.error_bitmap |= State::POWER_GOOD_ERROR;
                state.system_status = State::ERROR;
            }
        }
    }

    auto handle_temperature_conversion(uint16_t conversion_result,
                                       TemperatureSensor& sensor) {
        auto visitor = [this, &sensor](auto val) {
            this->visit_conversion(val, sensor);
        };
        sensor.last_adc = conversion_result;
        auto old_error = sensor.error;
        std::visit(visitor, sensor.conversion.convert(conversion_result));
        if (sensor.error != old_error) {
            if (sensor.error != errors::ErrorCode::NO_ERROR) {
                state.error_bitmap |= sensor.error_bit;
                auto error_message = messages::HostCommsMessage(
                    messages::ErrorMessage{.code = sensor.error});
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        error_message));
            } else {
                state.error_bitmap &= ~sensor.error_bit;
            }
        }
    }

    auto visit_conversion(thermistor_conversion::Error error,
                          TemperatureSensor& sensor) -> void {
        switch (error) {
            case thermistor_conversion::Error::OUT_OF_RANGE_LOW: {
                sensor.temp_c = 0;
                sensor.error = sensor.short_error;
            }
            case thermistor_conversion::Error::OUT_OF_RANGE_HIGH: {
                sensor.temp_c = 0;
                sensor.error = sensor.disconnected_error;
            }
        }
    }

    auto visit_conversion(double value, TemperatureSensor& sensor) -> void {
        if (value > sensor.overtemp_limit_c) {
            sensor.error = sensor.overtemp_error;
        } else {
            sensor.error = errors::ErrorCode::NO_ERROR;
        }
        sensor.temp_c = value;
    }

    [[nodiscard]] auto most_relevant_error() const -> errors::ErrorCode {
        // We have a lot of different errors from a lot of different sources.
        // Sometimes more than one can occur at the same time; sometimes, that
        // means that one has caused the other. We want to track them
        // separately, but we also sometimes want to respond with just one error
        // condition that sums everything up. This method is used by code that
        // wants the single most relevant code for the current error condition.
        if ((state.error_bitmap & State::SENSE_ERROR) != 0) {
            // Prefer sense errors since they'll be most specific
            if ((state.error_bitmap & State::PAD_SENSE_ERROR) != 0) {
                // Prefer pad a errors to pad b errors arbitrarily
                if ((state.error_bitmap & State::PAD_A_SENSE_ERROR) != 0) {
                    return pad_a.error;
                }
                return pad_b.error;
            }
        }

        // Return the heater pad error if everything is ok but the error latch
        // is set, which signifies that the latch circuit is broken
        if ((state.error_bitmap & State::POWER_GOOD_ERROR) != 0) {
            return errors::ErrorCode::HEATER_HARDWARE_ERROR_LATCH;
        }

        return board.error;
    }

    [[nodiscard]] auto pad_temperature() const -> double {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        return (pad_a.temp_c + pad_b.temp_c) / 2.0;
    }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    TemperatureSensor pad_a;
    TemperatureSensor pad_b;
    TemperatureSensor board;
    State state;
    PID pid;
    double setpoint;
};

};  // namespace heater_task
