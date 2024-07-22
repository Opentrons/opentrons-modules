#include "firmware/motor_policy.hpp"

#include "firmware/motor_hardware.h"

namespace motor_policy {

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto MotorPolicy::spi_transmit_receive(MotorID motor_id, uint8_t *tx_data,
                                       uint8_t *rx_data, uint16_t len) -> void {
    spi_dma_transmit_receive(motor_id, tx_data, rx_data, len);
}

}  // namespace motor_policy
