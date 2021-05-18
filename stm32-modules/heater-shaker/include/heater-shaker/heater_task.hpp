/*
 * the primary interface to the heater task
 */
#pragma once

#include <cstddef>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/errors.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater-shaker/thermistor_conversion.hpp"

/* Need a forward declaration for this because of recursive includes */
namespace tasks {
template <template <class> class Queue>
struct Tasks;
};

namespace heater_task {

struct State {
    enum Status {
        IDLE,
        ERROR,
    };
    Status system_status;
    uint8_t error_bitmap;
    static constexpr uint8_t PAD_A_SENSE_ERROR = (1 << 0);
    static constexpr uint8_t PAD_B_SENSE_ERROR = (1 << 1);
    static constexpr uint8_t PAD_SENSE_ERROR =
        PAD_A_SENSE_ERROR | PAD_B_SENSE_ERROR;
    static constexpr uint8_t BOARD_SENSE_ERROR = (1 << 2);
    static constexpr uint8_t SENSE_ERROR = PAD_SENSE_ERROR | BOARD_SENSE_ERROR;
};

struct TemperatureSensor {
    // The last converted temperature (0 if it was not valid)
    double temp_c = 0;
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
requires MessageQueue<QueueImpl<Message>, Message> class HeaterTask {
    using Queue = QueueImpl<Message>;

  public:
    static constexpr const uint32_t CONTROL_PERIOD_TICKS = 100;
    static constexpr double THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM = 44.2;
    static constexpr uint8_t ADC_BIT_DEPTH = 12;
    static constexpr double HEATER_PAD_OVERTEMP_SAFETY_LIMIT_C = 100;
    static constexpr double BOARD_OVERTEMP_SAFETY_LIMIT_C = 60;
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
          state{.system_status = State::IDLE, .error_bitmap = 0} {}
    HeaterTask(const HeaterTask& other) = delete;
    auto operator=(const HeaterTask& other) -> HeaterTask& = delete;
    HeaterTask(HeaterTask&& other) noexcept = delete;
    auto operator=(HeaterTask&& other) noexcept -> HeaterTask& = delete;
    ~HeaterTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }

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
     * */
    auto run_once() -> void {
        auto message = Message(std::monostate());

        // This is the call down to the provided queue. It will block for
        // anywhere up to the provided timeout, which drives the controller
        // frequency.

        static_cast<void>(message_queue.recv(&message));
        std::visit(
            [this](const auto& msg) -> void { this->visit_message(msg); },
            message);
    }

  private:
    auto visit_message(const std::monostate& _ignore) -> void {
        static_cast<void>(_ignore);
    }

    auto visit_message(const messages::SetTemperatureMessage& msg) -> void {
        if (state.system_status == State::ERROR) {
            // While in error state, we will refuse to set temperatures
            auto response = messages::AcknowledgePrevious{
                .responding_to_id = msg.id,
                .with_error = most_relevant_error()};
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));

        } else {
            auto response =
                messages::AcknowledgePrevious{.responding_to_id = msg.id};
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    messages::HostCommsMessage(response)));
        }
    }

    auto visit_message(const messages::GetTemperatureMessage& msg) -> void {
        errors::ErrorCode code = pad_a.error != errors::ErrorCode::NO_ERROR
                                     ? pad_a.error
                                     : pad_b.error;
        auto avg_temp =
            static_cast<int16_t>((static_cast<int32_t>(pad_a.temp_c) +
                                  static_cast<int32_t>(pad_b.temp_c)) /
                                 2);
        auto response = messages::GetTemperatureResponse{
            .responding_to_id = msg.id,
            .current_temperature = avg_temp,
            .setpoint_temperature =
                48,  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
            .with_error = code};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    auto visit_message(const messages::TemperatureConversionComplete& msg)
        -> void {
        handle_temperature_conversion(msg.pad_a, pad_a);
        handle_temperature_conversion(msg.pad_b, pad_b);
        handle_temperature_conversion(msg.board, board);
    }

    auto handle_temperature_conversion(uint16_t conversion_result,
                                       TemperatureSensor& sensor) {
        auto visitor = [this, &sensor](auto val) {
            this->visit_conversion(val, sensor);
        };
        auto old_error = sensor.error;
        std::visit(visitor, sensor.conversion.convert(conversion_result));
        if (sensor.error != old_error) {
            // if the error state of the sensor changed...
            if (sensor.error != errors::ErrorCode::NO_ERROR) {
                // ...to an error, send it and set state to error and set our
                // error bit
                state.system_status = State::ERROR;
                state.error_bitmap |= sensor.error_bit;
                auto error_message = messages::HostCommsMessage(
                    messages::ErrorMessage{.code = sensor.error});
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        error_message));
            } else {
                // ...to an ok state, clear our error bit in the task error map
                // and maybe switch to ok state
                state.error_bitmap &= ~sensor.error_bit;
                sensor.error = errors::ErrorCode::NO_ERROR;
                if (state.error_bitmap == 0) {
                    state.system_status = State::IDLE;
                }
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
            return board.error;
        }
        return errors::ErrorCode::NO_ERROR;
    }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    TemperatureSensor pad_a;
    TemperatureSensor pad_b;
    TemperatureSensor board;
    State state;
};

};  // namespace heater_task
