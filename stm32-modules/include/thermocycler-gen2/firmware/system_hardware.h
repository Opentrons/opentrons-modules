#ifndef SYSTEM_HARDWARE_H__
#define SYSTEM_HARDWARE_H__
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>

// Front button can only be pressed at 200ms increments
#define FRONT_BUTTON_DEBOUNCE_MS (200)
// Front button should be queried at this frequency after debouncing
#define FRONT_BUTTON_QUERY_RATE_MS (50)

// Type for callback when the button has been pressed
typedef void (*front_button_callback_t)(void);

typedef void (*systick_callback_t)(void);

/**
 * @brief Initialize the system hardware
 * @param[in] rev_1_board Set to true if this is a rev 1 board, false if it
 *                        is any other revision.
 * @param[in] button_cb Callback to invoke when the front button is pressed
 */
void system_hardware_setup(bool rev_1_board, front_button_callback_t button_cb);
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
 * @brief Turn the front button LED on or off
 *
 * @param set True to enable the LED, false to turn it off
 */
void system_front_button_led_set(bool set);

/**
 * @brief Hardcoded callback when the IRQ for the front button is triggered.
 * This function automatically performs debouncing logic so action is only
 * taken for every unique button press.
 */
void system_front_button_callback(void);

void system_hardware_jump_from_exception(void) __attribute__((naked));

void system_set_systick_callback(systick_callback_t cb);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _SYSTEM_HARDWARE_H__
