/**
 * @file tmc2130_policy.hpp
 * @brief Implements SPI
 *
 */
#pragma once

#include "firmware/motor_hardware.h"
#include "firmware/motor_spi_hardware.h"
#include "thermocycler-refresh/tmc2130.hpp"

class TMC2130Policy {
  public:
    using RxTxReturn = std::optional<tmc2130::MessageT>;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    auto tmc2130_transmit_receive(tmc2130::MessageT& data) -> RxTxReturn {
        tmc2130::MessageT retBuf = {0};
        if (motor_spi_sendreceive(data.data(), retBuf.data(), data.size())) {
            return RxTxReturn(retBuf);
        }
        return RxTxReturn();
    }
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    auto tmc2130_set_enable(bool enable) -> bool {
        return motor_hardware_set_seal_enable(enable);
    }

    auto tmc2130_set_direction(bool direction) -> bool {
        return motor_hardware_set_seal_direction(direction);
    }

    auto tmc2130_step_pulse() -> bool {
        motor_hardware_seal_step_pulse();
        return true;
    }
};
