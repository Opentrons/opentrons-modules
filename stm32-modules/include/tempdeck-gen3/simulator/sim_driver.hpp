#pragma once

#include <functional>

#include "simulator/simulator_queue.hpp"
#include "tempdeck-gen3/host_comms_task.hpp"
#include "tempdeck-gen3/messages.hpp"

namespace sim_driver {

using SendToCommsFunc = std::function<void(messages::IncomingMessageFromHost&)>;

class SimDriver {
  public:
    virtual const std::string& get_name() const = 0;
    virtual void write(const std::string& message) = 0;
    virtual void read(SendToCommsFunc&& send_to_comms) = 0;
};
}  // namespace sim_driver
