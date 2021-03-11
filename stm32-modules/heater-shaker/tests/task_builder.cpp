#include "test/task_builder.hpp"

TaskBuilder::TaskBuilder()
    : host_comms_queue("host comms"),
      host_comms_task(host_comms_queue),
      ui_queue("ui"),
      ui_task(ui_queue),
      motor_queue("motor"),
      motor_task(motor_queue),
      heater_queue("heater"),
      heater_task(heater_queue),
      task_aggregator{.heater = heater_task,
                      .comms = host_comms_task,
                      .motor = motor_task,
                      .ui = ui_task} {
    host_comms_task.provide_tasks(&task_aggregator);
    ui_task.provide_tasks(&task_aggregator);
    motor_task.provide_tasks(&task_aggregator);
    heater_task.provide_tasks(&task_aggregator);
}

auto TaskBuilder::build() -> std::shared_ptr<TaskBuilder> {
    return std::shared_ptr<TaskBuilder>(new TaskBuilder());
}
