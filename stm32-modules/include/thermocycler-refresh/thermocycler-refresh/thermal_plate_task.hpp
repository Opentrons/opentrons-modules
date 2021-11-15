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
    { p.set_enabled(false)};
};

constexpr uint16_t thermistorErrorBit(const ThermistorID id) { 
    if(id > THERM_HEATSINK) { throw std::out_of_range(""); }
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
    static constexpr uint8_t ADC_BIT_DEPTH = 16;
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

    explicit ThermalPlateTask(Queue& q) :
        message_queue(q),
        task_registry(nullptr),
        peltier_left{
            .id = PELTIER_LEFT,
            .enabled = false,
            .temp_current = 0.0,
            .temp_target = 0.0
        },
        peltier_right{
            .id = PELTIER_RIGHT,
            .enabled = false,
            .temp_current = 0.0,
            .temp_target = 0.0
        },
        peltier_center{
            .id = PELTIER_CENTER,
            .enabled = false,
            .temp_current = 0.0,
            .temp_target = 0.0
        },
        thermistors{ {
            {
                .overtemp_limit_c = OVERTEMP_LIMIT_C,
                .error_bit = thermistorErrorBit(THERM_FRONT_RIGHT)
            },
            {
                .overtemp_limit_c = OVERTEMP_LIMIT_C,
                .error_bit = thermistorErrorBit(THERM_FRONT_LEFT)
            },
            {
                .overtemp_limit_c = OVERTEMP_LIMIT_C,
                .error_bit = thermistorErrorBit(THERM_FRONT_CENTER)
            },
            {
                .overtemp_limit_c = OVERTEMP_LIMIT_C,
                .error_bit = thermistorErrorBit(THERM_BACK_RIGHT)
            },
            {
                .overtemp_limit_c = OVERTEMP_LIMIT_C,
                .error_bit = thermistorErrorBit(THERM_BACK_LEFT)
            },
            {
                .overtemp_limit_c = OVERTEMP_LIMIT_C,
                .error_bit = thermistorErrorBit(THERM_BACK_CENTER)
            },
            {
                .overtemp_limit_c = OVERTEMP_LIMIT_C,
                .error_bit = thermistorErrorBit(THERM_HEATSINK)
            }
        } },
        state{
            .system_status = State::IDLE,
            .error_bitmap = 0
        },
        plate_pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD, 
            CONTROL_PERIOD_SECONDS, 1.0, -1.0)
        {}
    ThermalPlateTask(const ThermalPlateTask &other) = delete;
    auto operator=(const ThermalPlateTask &other) -> ThermalPlateTask& = delete;
    ThermalPlateTask(ThermalPlateTask&& other) noexcept = delete;
    auto operator=(ThermalPlateTask&& other) noexcept -> ThermalPlateTask& = delete;
    ~ThermalPlateTask() = default;
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
     * ThermalPlateExecutionPolicy concept above.
     * */
    template <typename Policy>
    requires ThermalPlateExecutionPolicy<Policy>
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
    auto visit_message(const messages::ThermalPlateTempReadComplete& msg, 
                       Policy& policy) -> void {
        // TODO fill out this function
    }

    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    Peltier peltier_left;
    Peltier peltier_right;
    Peltier peltier_center;
    std::array<Thermistor, PLATE_THERM_COUNT> thermistors;
    State state;
    PID plate_pid;
};


}
