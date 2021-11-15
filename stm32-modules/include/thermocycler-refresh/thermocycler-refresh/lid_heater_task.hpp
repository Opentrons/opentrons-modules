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
    static constexpr uint8_t ADC_BIT_DEPTH = 16;
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
        : message_queue(q),
          task_registry(nullptr),
          thermistor{.overtemp_limit_c = OVERTEMP_LIMIT_C,
                     .error_bit = State::LID_THERMISTOR_ERROR},
          state{.system_status = State::IDLE, .error_bitmap = 0},
          pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, CONTROL_PERIOD_SECONDS, 1.0,
              -1.0) {}
    LidHeaterTask(const LidHeaterTask& other) = delete;
    auto operator=(const LidHeaterTask& other) -> LidHeaterTask& = delete;
    LidHeaterTask(LidHeaterTask&& other) noexcept = delete;
    auto operator=(LidHeaterTask&& other) noexcept -> LidHeaterTask& = delete;
    ~LidHeaterTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }

    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
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

        static_cast<void>(message_queue.recv(&message));
        std::visit(
            [this, &policy](const auto& msg) -> void {
                this->visit_message(msg, policy);
            },
            message);
    }

  private:
    template <typename Policy>
    auto visit_message(const std::monostate& _ignore, Policy& policy) -> void {
        static_cast<void>(policy);
        static_cast<void>(_ignore);
    }

    template <typename Policy>
    auto visit_message(const messages::LidTempReadComplete& msg, Policy& policy)
        -> void {
        // TODO fill out this function
    }

    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    Thermistor thermistor;
    State state;
    PID pid;
};

}  // namespace lid_heater_task
