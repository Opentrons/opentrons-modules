#ifndef I2C_HARDWARE_H_
#define I2C_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

typedef enum I2C_BUS {
    I2C_BUS_THERMAL,
    I2C_BUS_LED,
    I2C_BUS_COUNT,
} I2C_BUS;

#define IS_I2C_BUS(bus) (bus == I2C_BUS_THERMAL || bus == I2C_BUS_LED)

void i2c_hardware_init();

/**
 * @brief Writes a 16-bit value to an I2C bus
 * @note Thread safe
 * @warning This function should \e only be called from a FreeRTOS
 * task context! It relies on a mutex to lock the commmunication
 * @param[in] addr The device address to write to
 * @param[in] reg The register address to write to
 * @param[in] val The two-byte value to write. The MSB is the first
 * byte written, and the LSB is the latter byte written.
 * @return true on success, false  on error
 */
bool i2c_hardware_write_16(I2C_BUS bus, uint16_t addr, uint8_t reg,
                           uint16_t val);
/**
 * @brief Reads a 16-bit value from an I2C bus
 * @note Thread safe
 * @warning This function should \e only be called from a FreeRTOS
 * task context! It relies on a mutex to lock the commmunication!
 * @param[in] addr The device address to read from
 * @param[in] reg The register address to read from
 * @param[out] val Returns the two-byte value that was read. The MSB is
 * the first byte read, and the LSB is the latter byte read.
 * @return true on success, false  on error
 */
bool i2c_hardware_read_16(I2C_BUS bus, uint16_t addr, uint8_t reg,
                          uint16_t *val);

/**
 * @brief Writes an arbitrary array of data to a device
 * @note Thread safe
 *
 * @param addr I2C device address to write to
 * @param data Pointer to array of data to write
 * @param len Number of bytes in \c data
 * @return True if the write was succesful, false otherwise
 */
bool i2c_hardware_write_data(I2C_BUS bus, uint16_t addr, uint8_t *data,
                             uint16_t len);

/**
 * @brief Reads an arbitrary string of data from a device
 * @note Thread safe
 *
 * @param addr I2C device address to reaad from
 * @param data Pointer to array to store read data
 * @param len Number of bytes to read into \c data
 * @return True if the read was succesful, false otherwise
 */
bool i2c_hardware_read_data(I2C_BUS bus, uint16_t addr, uint8_t *data,
                            uint16_t len);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* I2C_HARDWARE_H_ */
