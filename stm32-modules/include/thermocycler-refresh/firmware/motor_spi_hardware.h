/**
 * @file motor_spi_hardware.h
 * @brief SPI interface to communicate with the Trinamic TMC2130 motor driver
 */

#ifndef MOTOR_SPI_HARDWARE_H__
#define MOTOR_SPI_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Initialize the SPI bus for the motor driver.*/
void motor_spi_initialize(void);

/**
 * @brief Sends & receives data over the SPI bus
 *
 * @param[in] in The buffer to write. Must contain \c len bytes.
 * @param[out] out Returns the \c len bytes read from the TMC2130
 * @return True on success, false on any error to write/read
 */
bool motor_spi_sendreceive(uint8_t *in, uint8_t *out, size_t len);

/** @brief This function handles SPI2 global interrupt. */
void SPI2_IRQHandler(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* MOTOR_SPI_HARDWARE_H__ */
