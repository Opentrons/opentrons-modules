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


    // Central aggregator
    using QueueAggregator = queue_aggregator::QueueAggregator<MotorDriverQueue, MotorQueue>;

    // Addresses
    static constexpr size_t MotorDriverAddress =
        QueueAggregator::template get_queue_idx<MotorDriverQueue>();
    static constexpr size_t MotorAddress =
        QueueAggregator::template get_queue_idx<MotorQueue>();
};

};  // namespace tasks
