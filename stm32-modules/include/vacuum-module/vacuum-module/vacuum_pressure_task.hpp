#include <array>
#include <cstdint>

#include "core/fixed_point.hpp"
#include "core/queue_aggregator.hpp"
#include "hal/message_queue.hpp"
#include "hardware_iface.hpp"
#include "mpr_pressure_sensor_policy.hpp"
#include "system_hardware.h"  // this is where I have the EOC pin set up rn
#include "systemwide.h"
#include "vacuum-module/MPRLL025PA00001A.hpp"
#include "vacuum-module/errors.hpp"
#include "vacuum-module/messages.hpp"
#include "vacuum-module/tasks.hpp"

namespace vacuum_pressure_sensor_task {
using namespace mpr_pressure::hardware;
using Message = messages::VacuumPressureMessage;

struct VacuumPressureSensor {
    VacuumPressureSensorId id = SensorA;
    mpr_pressure_sensor::MPRLL0025PA00001 driver;
    uint8_t message_id = 0;
};

const VacuumPressureSensor vacuum_sensor_a = {
    .id = SensorA,
    .driver = mpr_pressure_sensor::MPRLL0025PA00001(),
};

const VacuumPressureSensor vacuum_sensor_b = {
    .id = SensorB,
    .driver = mpr_pressure_sensor::MPRLL0025PA00001(),
};

template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class VacuumPressureSensorTask {
  private:
    using Queue = QueueImpl<Message>;
    using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
    using Queues = typename tasks::Tasks<QueueImpl>;

  public:
    explicit VacuumPressureSensorTask(Queue& q, Aggregator* aggregator,
                                      MPRPressureSensorPolicy* policy)
        : _message_queue(q), _task_registry(aggregator), _policy(policy) {}
    VacuumPressureSensorTask(const VacuumPressureSensorTask& other) = delete;
    auto operator=(const VacuumPressureSensorTask& other)
        -> VacuumPressureSensorTask& = delete;
    VacuumPressureSensorTask(VacuumPressureSensorTask&& other) noexcept =
        delete;
    auto operator=(VacuumPressureSensorTask&& other) noexcept
        -> VacuumPressureSensorTask& = delete;
    ~VacuumPressureSensorTask() = default;

    auto provide_aggregator(Aggregator* aggregator) {
        _task_registry = aggregator;
    }

    template <MPRPressureSensorPolicy Policy>
    auto run_once(Policy* policy) -> void {
        if (!task_registry) {
            return;
        }

        if (!initialized) {
            _policy = policy;
            // do startup stuff here
        }
    }
};

}  // namespace vacuum_pressure_sensor_task