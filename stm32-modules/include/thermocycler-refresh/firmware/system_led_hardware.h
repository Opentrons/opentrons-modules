#ifndef SYSTEM_LED_HARDWARE_H__
#define SYSTEM_LED_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the LED controller. This is a prerequisite to driving the
 * LED strip connected to the board.
 */
void system_led_iniitalize(void);

/** Callback for MSP initialization of the LED timer */
void system_led_msp_init(void);

/** Callback for MSP deinitialization of the LED timer */
void system_led_msp_deinit(void);

/**
 * @brief Begins a DMA transfer to the PWM timer for the LED's,
 * using a specified buffer of size \c len. The function initiates
 * a circular DMA transfer to the timer, and this transfer will
 * continue indefinitely until the \c system_led_stop function
 * is executed.
 *
 * @param[in] buffer The buffer of PWM values to send via DMA
 * @param[in] len The number of bytes in \c buffer
 * @return True if the transfer is started succesfully,
 * false otherwise
 */
bool system_led_start_send(uint16_t *buffer, size_t len);

/**
 * @brief Ends any active timer activity. The output line
 * will be set to \c 0V and will indicate to the XT1511 string
 * that there is no more data to read.
 */
void system_led_stop(void);

/**
 * @brief Get the maximum PWM value that can be used for the XT1511 control
 * @return The maximum 16-bit PWM compare value that corresponds to 100% on
 */
uint16_t system_led_max_pwm(void);

/**
 * @brief Wait for an interrupt on the PWM timer. This will
 * return after \e either a Full or Half Complete callback
 * is signalled by the DMA for the timer.
 * @param[in] timeout The max time to wait, in milliseconds
 * @return True if an interrupt is received, false if the
 * timeout is reached
 */
bool system_led_wait_for_interrupt(uint32_t timeout);

// Callback functions

/** Callback for TIM17 DMA IRQ handling.*/
void DMA1_Channel1_IRQHandler(void);

/** Callback for BOTH half and full pulse complete callback.*/
void system_led_pulse_callback(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* SYSTEM_LED_HARDWARE_H__ */
