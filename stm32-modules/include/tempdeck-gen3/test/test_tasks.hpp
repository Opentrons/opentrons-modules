#pragma once

#include "tempdeck-gen3/host_comms_task.hpp"
#include "tempdeck-gen3/system_task.hpp"
#include "tempdeck-gen3/ui_task.hpp"
#include "tempdeck-gen3/tasks.hpp"
#include "test/test_message_queue.hpp"

namespace tasks {

// Since the test target lacks a main.cpp to set up each task, we provide
// a class for it here.
class TestTasks {
  public:
    using Queues = Tasks<TestMessageQueue>;
    explicit TestTasks()
        : _comms_queue("comms"),
          _system_queue("system"),
          _ui_queue("ui"),
          _aggregator(_comms_queue, _system_queue),
          _comms_task(_comms_queue, &_aggregator),
          _system_task(_system_queue, &_aggregator),
          _ui_task(_ui_queue, &_aggregator) {}

    Queues::HostCommsQueue _comms_queue;
    Queues::SystemQueue _system_queue;
    Queues::UIQueue _ui_queue;
    Queues::QueueAggregator _aggregator;
    host_comms_task::HostCommsTask<TestMessageQueue> _comms_task;
    system_task::SystemTask<TestMessageQueue> _system_task;
    ui_task::UITask<TestMessageQueue> _ui_task;
};

static auto BuildTasks() -> TestTasks* { return new TestTasks(); }

};  // namespace tasks
