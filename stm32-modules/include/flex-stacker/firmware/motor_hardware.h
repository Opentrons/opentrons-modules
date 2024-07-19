#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus


#include <stdint.h>

void motor_spi2_init(void);
void motor_spi2_dma_sendreceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t len);
void motor_spi2_sendreceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t len);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
