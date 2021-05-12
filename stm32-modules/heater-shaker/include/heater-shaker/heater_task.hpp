/*
 * the primary interface to the heater task
 */
#pragma once

#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater-shaker/thermistor_conversion.hpp"

/* Need a forward declaration for this because of recursive includes */
namespace tasks {
template <template <class> class Queue>
struct Tasks;
};

namespace heater_task {
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
    explicit HeaterTask(Queue& q)
        : message_queue(q),
          task_registry(nullptr),
          pad_a_conversion(
              thermistor_conversion::ThermistorType::NTCG104ED104DTDSX,
              THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_DEPTH),
          pad_b_conversion(
              thermistor_conversion::ThermistorType::NTCG104ED104DTDSX,
              THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_DEPTH),
          board_conversion(
              thermistor_conversion::ThermistorType::NTCG104ED104DTDSX,
              THERMISTOR_CIRCUIT_BIAS_RESISTANCE_KOHM, ADC_BIT_DEPTH),
          pad_a_temperature(0),
          pad_b_temperature(0),
          board_temperature(0) {}
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
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    auto visit_message(const messages::GetTemperatureMessage& msg) -> void {
        auto avg_temp =
            static_cast<int16_t>((static_cast<int32_t>(pad_a_temperature) +
                                  static_cast<int32_t>(pad_b_temperature)) /
                                 2);
        auto response = messages::GetTemperatureResponse{
            .responding_to_id = msg.id,
            .current_temperature = avg_temp,
            .setpoint_temperature =
                48};  // NOLINT(cppcoreguidelines-avoid-magic-numbers)
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    auto visit_message(const messages::TemperatureConversionComplete& msg)
        -> void {
        auto visitor = [this](auto val) { return this->visit_conversion(val); };
        pad_a_temperature =
            std::visit(visitor, pad_a_conversion.convert(msg.pad_a));
        pad_b_temperature =
            std::visit(visitor, pad_b_conversion.convert(msg.pad_b));
        board_temperature =
            std::visit(visitor, board_conversion.convert(msg.board));
    }

    auto visit_conversion(thermistor_conversion::Error error) -> double {
        switch (error) {
            case thermistor_conversion::Error::OUT_OF_RANGE_LOW: {
                auto message =
                    messages::HostCommsMessage(messages::ErrorMessage{
                        .code = errors::ErrorCode::
                            HEATER_THERMISTOR_OUT_OF_RANGE_LOW});
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        message));
                break;
            }
            case thermistor_conversion::Error::OUT_OF_RANGE_HIGH: {
                auto message =
                    messages::HostCommsMessage(messages::ErrorMessage{
                        .code = errors::ErrorCode::
                            HEATER_THERMISTOR_OUT_OF_RANGE_HIGH});
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        message));
                break;
            }
        }
        return 0;
    }

    auto visit_conversion(double value) -> double { return value; }
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    const thermistor_conversion::Conversion pad_a_conversion;
    const thermistor_conversion::Conversion pad_b_conversion;
    const thermistor_conversion::Conversion board_conversion;
    int16_t pad_a_temperature;
    int16_t pad_b_temperature;
    int16_t board_temperature;
};

};  // namespace heater_task
