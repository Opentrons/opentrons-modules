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

void hw_step_motor(MotorID motor_id);
bool hw_enable_motor(MotorID motor_id);
void hw_start_motor_timer(MotorID motor_id);
bool hw_disable_motor(MotorID motor_id);
bool hw_stop_motor(MotorID motor_id);
void hw_set_direction(MotorID, bool direction);
bool hw_read_home_sensor(MotorID motor_id);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H

#pragma GCC diagnostic push
// NOLINTNEXTLINE(clang-diagnostic-unknown-warning-option)
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic pop

/******************* Motor R *******************/

/** Motor hardware **/
#define R_STEP_PIN (GPIO_PIN_8)
#define R_STEP_PORT (GPIOA)
#define R_DIR_PIN (GPIO_PIN_1)
#define R_DIR_PORT (GPIOB)
#define R_EN_PIN (GPIO_PIN_10)
#define R_EN_PORT (GPIOA)
