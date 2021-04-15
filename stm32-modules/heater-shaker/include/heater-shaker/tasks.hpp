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
#include "ui_task.hpp"

namespace motor_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::MotorMessage>,
                      messages::MotorMessage> class MotorTask;
}

namespace heater_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::HeaterMessage>,
                      messages::HeaterMessage> class HeaterTask;
}

namespace host_comms_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::HostCommsMessage>,
                      messages::HostCommsMessage> class HostCommsTask;
}

namespace ui_task {
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<messages::UIMessage>,
                      messages::UIMessage> class UITask;
}

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
          ui_task::UITask<QueueImpl>* ui_in):
      heater(nullptr), comms(nullptr), motor(nullptr), ui(nullptr) {
        initialize(heater_in, comms_in, motor_in, ui_in);
    }

    auto initialize(heater_task::HeaterTask<QueueImpl>* heater_in,
                    host_comms_task::HostCommsTask<QueueImpl>* comms_in,
                    motor_task::MotorTask<QueueImpl>* motor_in,
                    ui_task::UITask<QueueImpl>* ui_in) -> void {
        heater = heater_in;
        comms = comms_in;
        motor = motor_in;
        ui = ui_in;
        heater->provide_tasks(this);
        comms->provide_tasks(this);
        motor->provide_tasks(this);
        ui->provide_tasks(this);
    }

    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    heater_task::HeaterTask<QueueImpl>* heater;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    host_comms_task::HostCommsTask<QueueImpl>* comms;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    motor_task::MotorTask<QueueImpl>* motor;
    // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes)
    ui_task::UITask<QueueImpl>* ui;
};
}  // namespace tasks
