#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

#include "systemwide.h"

void motor_hardware_init(void);

void spi_hardware_init(void);
bool motor_spi_sendreceive(MotorID motor_id, uint8_t *tx_data, uint8_t *rx_data,
                           uint16_t len);

void step_motor(MotorID motor_id);
void unstep_motor(MotorID motor_id);
void hw_enable_motor(MotorID motor_id);
void hw_disable_motor(MotorID motor_id);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
