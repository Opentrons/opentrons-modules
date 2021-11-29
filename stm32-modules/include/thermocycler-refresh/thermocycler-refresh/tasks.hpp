/*
** Defines the tasks struct that holds pointers to all the different tasks in
*the system
*/

#pragma once

#include "hal/message_queue.hpp"
#include "host_comms_task.hpp"
#include "lid_heater_task.hpp"
#include "messages.hpp"
#include "system_task.hpp"
#include "thermal_plate_task.hpp"
#include "motor_task.hpp"

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

namespace lid_heater_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::LidHeaterMessage>,
                      messages::LidHeaterMessage>
class LidHeaterTask;
}  // namespace lid_heater_task

namespace motor_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::MotorMessage>,
                      messages::MotorMessage>
class MotorTask;
}  // namespace motor_task

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
          thermal_plate_task::ThermalPlateTask<QueueImpl>* thermal_plate_in,
          lid_heater_task::LidHeaterTask<QueueImpl>* lid_heater_in,
          motor_task::MotorTask<QueueImpl>* motor_in)
        : comms(nullptr),
          system(nullptr),
          thermal_plate(nullptr),
          lid_heater(nullptr),
          motor(nullptr) {
        initialize(comms_in, system_in, thermal_plate_in, lid_heater_in, motor_in);
    }

    auto initialize(
        host_comms_task::HostCommsTask<QueueImpl>* comms_in,
        system_task::SystemTask<QueueImpl>* system_in,
        thermal_plate_task::ThermalPlateTask<QueueImpl>* thermal_plate_in,
        lid_heater_task::LidHeaterTask<QueueImpl>* lid_heater_in,
        motor_task::MotorTask<QueueImpl>* motor_in) -> void {
        comms = comms_in;
        system = system_in;
        thermal_plate = thermal_plate_in;
        lid_heater = lid_heater_in;
        motor = motor_in;
        comms->provide_tasks(this);
        system->provide_tasks(this);
        thermal_plate_in->provide_tasks(this);
        lid_heater->provide_tasks(this);
        motor->provide_tasks(this);
    }

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    host_comms_task::HostCommsTask<QueueImpl>* comms;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    system_task::SystemTask<QueueImpl>* system;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    thermal_plate_task::ThermalPlateTask<QueueImpl>* thermal_plate;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    lid_heater_task::LidHeaterTask<QueueImpl>* lid_heater;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    motor_task::MotorTask<QueueImpl>* motor;
};
}  // namespace tasks
