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

    template<MessageQueue QueueImpl>
    class HeaterTask {
        public:
            MessageQueue<Message> message_queue;
        private:
    };
}
