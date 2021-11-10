#include "test/task_builder.hpp"

TaskBuilder::TaskBuilder()
    : host_comms_queue("host comms"),
      host_comms_task(host_comms_queue),
      system_queue("system"),
      system_task(system_queue),
      task_aggregator(&host_comms_task, &system_task),
      system_policy() {}

auto TaskBuilder::build() -> std::shared_ptr<TaskBuilder> {
    return std::shared_ptr<TaskBuilder>(new TaskBuilder());
}
