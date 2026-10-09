/**
 * @file tasks.hpp
 * @brief Generic tasks declaration
 */
#pragma once

#include "core/queue_aggregator.hpp"
#include "incubator-module/messages.hpp"

namespace tasks {

template <template <class> class QueueImpl>
struct Tasks {
    // Message queue for motor driver task
    using MotorDriverQueue = QueueImpl<messages::MotorDriverMessage>;
    // Message queue for motor task
    using MotorQueue = QueueImpl<messages::MotorMessage>;
    // Message queue for host comms
    using HostCommsQueue = QueueImpl<messages::HostCommsMessage>;
    // Message queue for system task
    using SystemQueue = QueueImpl<messages::SystemMessage>;
    // Message queue for UI task
    using UIQueue = QueueImpl<messages::UIMessage>;
    // Message queue for capacitive task
    using CapacitiveQueue = QueueImpl<messages::CapacitiveMessage>;
    // Message queue for proximity task
    using ProximityQueue = QueueImpl<messages::ProximityMessage>;

    // Central aggregator
    using QueueAggregator =
        queue_aggregator::QueueAggregator<MotorDriverQueue, MotorQueue, HostCommsQueue, SystemQueue, UIQueue,
                                          CapacitiveQueue, ProximityQueue>;

    // Addresses
    static constexpr size_t MotorDriverAddress =
        QueueAggregator::template get_queue_idx<MotorDriverQueue>();
    static constexpr size_t MotorAddress =
        QueueAggregator::template get_queue_idx<MotorQueue>();
    static constexpr size_t HostCommsAddress =
        QueueAggregator::template get_queue_idx<HostCommsQueue>();
    static constexpr size_t SystemAddress =
        QueueAggregator::template get_queue_idx<SystemQueue>();
    static constexpr size_t UIAddress =
        QueueAggregator::template get_queue_idx<UIQueue>();
    static constexpr size_t CapacitiveAddress =
        QueueAggregator::template get_queue_idx<CapacitiveQueue>();
    static constexpr size_t ProximityAddress =
        QueueAggregator::template get_queue_idx<ProximityQueue>();
};

};  // namespace tasks
