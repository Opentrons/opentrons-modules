/*
** Defines the tasks struct that holds pointers to all the different tasks in
*the system
*/

#pragma once

#include "hal/message_queue.hpp"
#include "heater_task.hpp"
#include "host_comms_task.hpp"
#include "messages.hpp"
#include "motor_task.hpp"
#include "system_task.hpp"

namespace motor_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::MotorMessage>, messages::MotorMessage>
class MotorTask;
}  // namespace motor_task

namespace heater_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::HeaterMessage>,
                      messages::HeaterMessage>
class HeaterTask;
}  // namespace heater_task

namespace host_comms_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::HostCommsMessage>,
                      messages::HostCommsMessage>
class HostCommsTask;
}  // namespace host_comms_task

namespace system_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::SystemMessage>,
                      messages::SystemMessage>
class SystemTask;
}  // namespace system_task

namespace tasks {
/* Container relating the RTOSTask for the implementation and the portable task
 * object */
template <typename RTOSHandle, class PortableTask>
struct Task {
    RTOSHandle handle;
    PortableTask* task;
};

/* Aggregator for initialized task objects that can be injected into those
 objects after they are created */
template <template <class> class QueueImpl>
struct Tasks {
    Tasks() = default;
    Tasks(heater_task::HeaterTask<QueueImpl>* heater_in,
          host_comms_task::HostCommsTask<QueueImpl>* comms_in,
          motor_task::MotorTask<QueueImpl>* motor_in,
          system_task::SystemTask<QueueImpl>* system_in)
        : heater(nullptr), comms(nullptr), motor(nullptr), system(nullptr) {
        initialize(heater_in, comms_in, motor_in, system_in);
    }

    auto initialize(heater_task::HeaterTask<QueueImpl>* heater_in,
                    host_comms_task::HostCommsTask<QueueImpl>* comms_in,
                    motor_task::MotorTask<QueueImpl>* motor_in,
                    system_task::SystemTask<QueueImpl>* system_in) -> void {
        heater = heater_in;
        comms = comms_in;
        motor = motor_in;
        system = system_in;
        heater->provide_tasks(this);
        comms->provide_tasks(this);
        motor->provide_tasks(this);
        system->provide_tasks(this);
    }

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    heater_task::HeaterTask<QueueImpl>* heater;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    host_comms_task::HostCommsTask<QueueImpl>* comms;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    motor_task::MotorTask<QueueImpl>* motor;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    system_task::SystemTask<QueueImpl>* system;
};
}  // namespace tasks
