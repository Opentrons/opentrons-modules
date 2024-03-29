/**
 * @file tasks.hpp
 * @brief Generic tasks declaration
 */
#pragma once

#include "core/queue_aggregator.hpp"
#include "tempdeck-gen3/messages.hpp"

namespace tasks {

template <template <class> class QueueImpl>
struct Tasks {
    // Message queue for host comms
    using HostCommsQueue = QueueImpl<messages::HostCommsMessage>;
    // Message queue for system task
    using SystemQueue = QueueImpl<messages::SystemMessage>;
    // Message queue for UI task
    using UIQueue = QueueImpl<messages::UIMessage>;
    // Message queue for Thermal Control Task
    using ThermalQueue = QueueImpl<messages::ThermalMessage>;

    // Central aggregator
    using QueueAggregator =
        queue_aggregator::QueueAggregator<HostCommsQueue, SystemQueue, UIQueue,
                                          ThermalQueue>;

    // Addresses
    static constexpr size_t HostAddress =
        QueueAggregator::template get_queue_idx<HostCommsQueue>();
    static constexpr size_t SystemAddress =
        QueueAggregator::template get_queue_idx<SystemQueue>();
    static constexpr size_t UIAddress =
        QueueAggregator::template get_queue_idx<UIQueue>();
    static constexpr size_t ThermalAddress =
        QueueAggregator::template get_queue_idx<ThermalQueue>();
};

};  // namespace tasks
