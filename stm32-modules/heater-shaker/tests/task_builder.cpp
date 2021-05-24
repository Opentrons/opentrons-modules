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
      task_aggregator(&heater_task, &host_comms_task, &motor_task, &ui_task),
      motor_policy(),
      heater_policy() {}

auto TaskBuilder::build() -> std::shared_ptr<TaskBuilder> {
    return std::shared_ptr<TaskBuilder>(new TaskBuilder());
}
