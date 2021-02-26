/*
 * the primary interface to the heater task
 */
#pragma once

#include <variant>

#include "hal/message_queue.hpp"

namespace heater_task {
    struct SetTempMessage {
        uint32_t target_temperature;
    };
    struct DeactivateMessage {

    };

    using Message=::std::variant<SetTempMessage, DeactivateMessage>;

    // By using a template template parameter here, we allow the code instantiating
    // this template to do so as HeaterTask<SomeQueueImpl> rather than
    // HeaterTask<SomeQueueImpl<Message>>
    template<template<class> class QueueImpl> requires MessageQueue<QueueImpl<Message>, Message>
    class HeaterTask {
        using Queue = QueueImpl<Message>;
        public:
            explicit HeaterTask(Queue& q): message_queue(q) {}
            HeaterTask(const HeaterTask& other) = delete;
            auto operator=(const HeaterTask& other) -> HeaterTask& = delete;
            HeaterTask(HeaterTask&& other) noexcept = delete;
            auto operator=(HeaterTask&& other) noexcept -> HeaterTask& = delete;
            ~HeaterTask() = default;
            auto get_message_queue() -> Queue& {return message_queue;}
        private:
            Queue& message_queue;
    };

};
