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
    PortableTask& task;
};

/* Aggregator for initialized task objects that can be injected into those
 objects after they are created */
template <template <class> class QueueImpl>
struct Tasks {
    heater_task::HeaterTask<QueueImpl>& heater;
    host_comms_task::HostCommsTask<QueueImpl>& comms;
    motor_task::MotorTask<QueueImpl>& motor;
    ui_task::UITask<QueueImpl>& ui;
};
}  // namespace tasks
