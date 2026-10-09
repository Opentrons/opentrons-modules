/**
 * @file usb_hardware.h
 * @brief Header with forward definitions of functions
 * for firmware-specific USB control code.
 */

#ifndef _USB_HARDWARE_C_
#define _USB_HARDWARE_C_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdint.h>

/**
 * @brief Function pointer type to be invoked when a new
 * packet is received.
 * @details
 * - First parameter contains a pointer to the buffer of
 * data returned
 * - Second parameter contains the length of the data
 * returned
 * - The return value should be a pointer to the buffer
 * where the next packet of RX data shall be stored
 */
typedef uint8_t *(*usb_rx_callback_t)(uint8_t *, uint32_t *);

/**
 * @brief Function pointer used for specifying callback
 * for CDC initialization
 * @details
 * - Should return a pointer to a buffer to store RX packets
 */
typedef uint8_t *(*usb_cdc_init_callback_t)();

/**
 * @brief Function pointer used for specifying callback
 * for CDC deinitialization
 */
typedef void (*usb_cdc_deinit_callback_t)();

/**
 * @brief Initializes the USB hardware on the system. Provides function
 * pointers to the C code that will be invoked upon certain USB CDC events.
 * @param[in] rx_cb The function to call when a USB packet arrives
 * @param[in] cdc_init_cb Function to call when initializing CDC
 * @param[in] cdc_deinit_cb Function to call when deinitializing CDC
 */
void usb_hw_init(usb_rx_callback_t rx_cb, usb_cdc_init_callback_t cdc_init_cb,
                 usb_cdc_deinit_callback_t cdc_deinit_cb);

/**
 * @brief Starts USB CDC on the system
 */
void usb_hw_start();

/**
 * @brief Stop USB
 */
void usb_hw_stop();

/**
 * @brief Send a packet over USB CDC
 * @param[in] buf The buffer to send
 * @param[in] len The length of the buffer to be sent
 */
void usb_hw_send(uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* _USB_HARDWARE_C_ */
