#pragma once

#include <optional>

#include "flex-stacker/tmc2160_interface.hpp"
#include "systemwide.h"

namespace motor_driver_policy {

class MotorDriverPolicy {
  public:
    using RxTxReturn = std::optional<tmc2160::MessageT>;
    auto tmc2160_transmit_receive(MotorID motor_id, tmc2160::MessageT& data)
        -> RxTxReturn;
};

}  // namespace motor_driver_policy
