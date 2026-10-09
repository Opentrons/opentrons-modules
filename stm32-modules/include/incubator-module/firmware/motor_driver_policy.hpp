#pragma once

#include <optional>

#include "incubator-module/tmc5160_interface.hpp"
#include "systemwide.h"

namespace motor_driver_policy {

class MotorDriverPolicy {
  public:
    using RxTxReturn = std::optional<tmc5160::MessageT>;
    auto tmc5160_transmit_receive(MotorID motor_id, tmc5160::MessageT& data)
        -> RxTxReturn;
};

}  // namespace motor_driver_policy
