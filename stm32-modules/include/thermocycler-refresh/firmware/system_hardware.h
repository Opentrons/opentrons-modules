#ifndef SYSTEM_HARDWARE_H__
#define SYSTEM_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>

// Type for callback when the button has been pressed
typedef void (*front_button_callback_t)(void);

/**
 * @brief Initialize the system hardware
 *
 */
void system_hardware_setup(front_button_callback_t button_cb);
/**
 * @brief Toggle the debug LED (hearbeat LED)
 *
 * @param set True to set the led on, false to disable it
 */
void system_debug_led(int set);
/**
 * @brief Enter the bootloader. This function never returns.
 */
void system_hardware_enter_bootloader(void);

/**
 * @brief Interrupt servicing for the HAL systick interrupt
 *
 */
void hal_timebase_tick(void);

/**
 * @brief Read the front button
 *
 * @return true if the button is pressed, false otherwise
 */
bool system_front_button_pressed(void);

/**
 * @brief Hardcoded callback when the IRQ for the front button is triggered.
 * This function automatically performs debouncing logic so action is only
 * taken for every unique button press.
 */
void system_front_button_callback(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _SYSTEM_HARDWARE_H__
