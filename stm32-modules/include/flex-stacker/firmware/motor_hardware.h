#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdint.h>
#include "systemwide.h"

void motor_hardware_init(void);

void spi_hardware_init(void);
void spi_dma_transmit_receive(MotorID motor_id, uint8_t *tx_data, uint8_t *rx_data,
                                uint16_t len);


#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
