/**
 * @file tasks.hpp
 * @brief Generic tasks declaration
 */
#pragma once

#include "core/queue_aggregator.hpp"
#include "flex-stacker/messages.hpp"

namespace tasks {

template <template <class> class QueueImpl>
struct Tasks {
    // Message queue for motor driver task
    using MotorDriverQueue = QueueImpl<messages::MotorDriverMessage>;
    // Message queue for motor control task
    using MotorQueue = QueueImpl<messages::MotorMessage>;
    // Message queue for host comms
    using HostCommsQueue = QueueImpl<messages::HostCommsMessage>;
    // Message queue for system task
    using SystemQueue = QueueImpl<messages::SystemMessage>;
    // Central aggregator
    using QueueAggregator =
        queue_aggregator::QueueAggregator<MotorDriverQueue, MotorQueue,
                                          HostCommsQueue, SystemQueue>;

    // Addresses
    static constexpr size_t MotorDriverAddress =
        QueueAggregator::template get_queue_idx<MotorDriverQueue>();
    static constexpr size_t MotorAddress =
        QueueAggregator::template get_queue_idx<MotorQueue>();
    static constexpr size_t HostCommsAddress =
        QueueAggregator::template get_queue_idx<HostCommsQueue>();
    static constexpr size_t SystemAddress =
        QueueAggregator::template get_queue_idx<SystemQueue>();
};

};  // namespace tasks
