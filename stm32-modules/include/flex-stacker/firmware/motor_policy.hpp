#pragma once

#include <cstdint>

#include "systemwide.h"

namespace motor_policy {

class MotorPolicy {
    public:
        auto enable_motor(MotorID motor_id) -> void;
        auto disable_motor(MotorID motor_id) -> void;
        auto set_motor_speed(MotorID motor_id, double speed) -> bool;

        auto spi_transmit_receive(MotorID motor_id, uint8_t *tx_data, uint8_t *rx_data,
                                  uint16_t len) -> void;
    };
}

