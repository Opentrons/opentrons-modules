/*
** Defines the tasks struct that holds pointers to all the different tasks in
*the system
*/

#pragma once

#include "hal/message_queue.hpp"
#include "host_comms_task.hpp"
#include "messages.hpp"
#include "system_task.hpp"
#include "thermal_plate_task.hpp"

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

namespace thermal_plate_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::ThermalPlateMessage>,
                      messages::ThermalPlateMessage>
class ThermalPlateTask;
}  // namespace thermal_plate_task

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
    Tasks(host_comms_task::HostCommsTask<QueueImpl>* comms_in,
          system_task::SystemTask<QueueImpl>* system_in,
          thermal_plate_task::ThermalPlateTask<QueueImpl>* thermal_plate)
        : comms(nullptr), system(nullptr), thermal_plate(nullptr) {
        initialize(comms_in, system_in, thermal_plate);
    }

    auto initialize(
        host_comms_task::HostCommsTask<QueueImpl>* comms_in,
        system_task::SystemTask<QueueImpl>* system_in,
        thermal_plate_task::ThermalPlateTask<QueueImpl>* thermal_plate_in)
        -> void {
        comms = comms_in;
        system = system_in;
        thermal_plate = thermal_plate_in;
        comms->provide_tasks(this);
        system->provide_tasks(this);
        thermal_plate_in->provide_tasks(this);
    }

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    host_comms_task::HostCommsTask<QueueImpl>* comms;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    system_task::SystemTask<QueueImpl>* system;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    thermal_plate_task::ThermalPlateTask<QueueImpl>* thermal_plate;
};
}  // namespace tasks
