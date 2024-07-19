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

    // Message queue for motor control stask
    using MotorQueue = QueueImpl<messages::MotorMessage>;

    // Central aggregator
    using QueueAggregator =
        queue_aggregator::QueueAggregator<MotorQueue>;

    // Addresses
    static constexpr size_t MotorAddress =
        QueueAggregator::template get_queue_idx<MotorQueue>();
};

};  // namespace tasks
