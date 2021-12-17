#pragma once

#include "firmware/motor_spi_hardware.h"
#include "thermocycler-refresh/tmc2130.hpp"

class MotorSpiPolicy {
  public:
    using ReadRT = std::optional<tmc2130::RegisterSerializedType>;
    auto transmit_receive(tmc2130::MessageT& data)
        -> std::optional<tmc2130::MessageT> {
        using RT = std::optional<tmc2130::MessageT>;
        tmc2130::MessageT retBuf = {0};
        if (motor_spi_sendreceive(data.data(), retBuf.data(), data.size())) {
            return RT(retBuf);
        }
        return RT(0);
    }
};
