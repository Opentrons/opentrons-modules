/**
* @file motor_task.hpp
* @brief Primary interface for the motor task
*
*/
#pragma once

#include "core/ack_cache.hpp"
#include "core/queue_aggregator.hpp"
#include "core/version.hpp"
#include "hal/message_queue.hpp"
#include "flex-stacker/messages.hpp"
#include "flex-stacker/tasks.hpp"

namespace motor_task {


using Message = messages::MotorMessage;

template <template <class> class QueueImpl>
   requires MessageQueue<QueueImpl<Message>, Message>
class MotorTask {
 private:
   using Queue = QueueImpl<Message>;
   using Aggregator = typename tasks::Tasks<QueueImpl>::QueueAggregator;
   using Queues = typename tasks::Tasks<QueueImpl>;

 public:
   explicit MotorTask(Queue& q, Aggregator* aggregator)
       : _message_queue(q),
         _task_registry(aggregator) {}
   MotorTask(const MotorTask& other) = delete;
   auto operator=(const MotorTask& other) -> MotorTask& = delete;
   MotorTask(MotorTask&& other) noexcept = delete;
   auto operator=(MotorTask&& other) noexcept -> MotorTask& = delete;
   ~MotorTask() = default;

   auto provide_aggregator(Aggregator* aggregator) {
       _task_registry = aggregator;
   }

   auto run_once() -> void {
       auto message = Message(std::monostate());

       _message_queue.recv(&message);
       auto visit_helper = [this](auto& message) -> void {
           this->visit_message(message);
       };
       std::visit(visit_helper, message);
   }

 private:

   auto visit_message(const std::monostate& message) -> void {
       static_cast<void>(message);
   }

   Queue& _message_queue;
   Aggregator* _task_registry;
};

};  // namespace motor_task
