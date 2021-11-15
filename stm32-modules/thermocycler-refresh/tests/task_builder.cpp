#include "test/task_builder.hpp"

TaskBuilder::TaskBuilder()
    : host_comms_queue("host comms"),
      host_comms_task(host_comms_queue),
      system_queue("system"),
      system_task(system_queue),
      thermal_plate_queue("thermal plate"),
      thermal_plate_task(thermal_plate_queue),
      lid_heater_queue("lid heater"),
      lid_heater_task(lid_heater_queue),
      task_aggregator(&host_comms_task, &system_task, &thermal_plate_task,
                      &lid_heater_task),
      system_policy(),
      thermal_plate_policy(),
      lid_heater_policy() {}

auto TaskBuilder::build() -> std::shared_ptr<TaskBuilder> {
    return std::shared_ptr<TaskBuilder>(new TaskBuilder());
}
