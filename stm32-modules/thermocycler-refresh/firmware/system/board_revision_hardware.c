/**
 * @file board_revision_hardware.c
 * @brief Firmware implementation of board revision checking.
 * @details The revision number of the board is detected through a set
 * of three GPIO inputs. On each revision, each of the pins either has
 * a pullup resistor, a pulldown resistor, or is left floating. To detect
 * each of these three states, the firmware alternates between using internal
 * pullups and pulldowns and reads the value of the input. If the input changes
 * based off of the value of the pullup, it is floating; otherwise the pin is
 * either pulled up or down.
 * 
 * In order for this scheme to work, the resistors must be relatively strong,
 * generally on the order of a kilohm.
 */

// My header
#include "thermocycler-refresh/board_revision_hardware.h"
// Library headers
#include <stdbool.h>
#include <stdint.h>
// Repo headers
#include "firmware/hal_util.h"

// Static variables 

// GPIO configuration for the revision pins
const static GPIOConfig _rev_gpio[BOARD_REV_PIN_COUNT] = {
    {.port = GPIOE, .pin = GPIO_PIN_9},
    {.port = GPIOE, .pin = GPIO_PIN_13},
    {.port = GPIOE, .pin = GPIO_PIN_14}
};

// Static function declarations

/**
 * @brief Reads an input to check for whether it is pulled up, pulled down,
 * or left floating
 * 
 * @param gpio The GPIO to read
 * @return The status of the GPIO
 */
static TrinaryInput_t read_input(const GPIOConfig *gpio);

// Public function implementation

void board_revision_read_inputs(TrinaryInput_t *inputs) {
    if(!inputs) { return; }
    __HAL_RCC_GPIOE_CLK_ENABLE();

    inputs[0] = read_input(&_rev_gpio[0]);
    inputs[1] = read_input(&_rev_gpio[1]);
    inputs[2] = read_input(&_rev_gpio[2]);
}

// Static function implementation

// Configure as pulldown, then pullup, and compare the results
static TrinaryInput_t read_input(const GPIOConfig *gpio) {
    if(!gpio) { return INPUT_FLOATING; }

    GPIO_PinState reading_low, reading_high;
    GPIO_InitTypeDef init = {
        .Mode = GPIO_MODE_INPUT,
        .Pin = gpio->pin,
        .Pull = GPIO_PULLDOWN,
        .Speed = GPIO_SPEED_LOW
    };
    HAL_GPIO_Init(gpio->port, &init);
    reading_low = HAL_GPIO_ReadPin(gpio->port, gpio->pin);

    init.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(gpio->port, &init);
    reading_high = HAL_GPIO_ReadPin(gpio->port, gpio->pin);
    HAL_GPIO_DeInit(gpio->port, gpio->pin);

    if(reading_low != reading_high) {
        return INPUT_FLOATING;
    }
    return (reading_low == GPIO_PIN_RESET) ? INPUT_PULLDOWN : INPUT_PULLUP;
}
