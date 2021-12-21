/**
 * @file board_revision.c
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
#include "thermocycler-refresh/board_revision.h"
// Library headers
#include <stdbool.h>
#include <stdint.h>
// Repo headers
#include "firmware/hal_util.h"

// Local definitions 
#define BOARD_REV_PIN_COUNT (3)

// Enumeration of GPIO input types - can be pulled up/down OR left floating
typedef enum TrinaryInput {
    INPUT_PULLDOWN,
    INPUT_PULLUP,
    INPUT_FLOATING
} TrinaryInput_t;

// Holds the expected inputs for a board rev
struct BoardRevSetting {
    TrinaryInput_t pins[BOARD_REV_PIN_COUNT];
    BoardRevision_t revision;
};

// Static variables 

// GPIO configuration for the revision pins
const static GPIOConfig _rev_gpio[BOARD_REV_PIN_COUNT] = {
    {.port = GPIOE, .pin = GPIO_PIN_9},
    {.port = GPIOE, .pin = GPIO_PIN_13},
    {.port = GPIOE, .pin = GPIO_PIN_14}
};
// Expected GPIO inputs for each board rev
const static struct BoardRevSetting _revisions[BOARD_REV_INVALID] = {
    {   
        .pins = { INPUT_FLOATING, INPUT_FLOATING, INPUT_FLOATING}, 
        .revision = BOARD_REV_1
    },
    {
        .pins = { INPUT_PULLDOWN, INPUT_PULLDOWN, INPUT_PULLDOWN}, 
        .revision = BOARD_REV_2
    }
};

// Board rev only needs to be checked once
static bool _has_been_checked = false;
// The actual revision
static BoardRevision_t _revision = BOARD_REV_INVALID;

// Static function declarations

/**
 * @brief Reads an input to check for whether it is pulled up, pulled down,
 * or left floating
 * 
 * @param gpio The GPIO to read
 * @return The status of the GPIO
 */
static TrinaryInput_t read_input(const GPIOConfig *gpio);
/**
 * @brief Look up which board revision we have, based off of the values
 * of the revision inputs
 * 
 * @param inputs An array of BOARD_REV_PIN_COUNT inputs to read
 * @return Which revision this board is, or BOARD_REV_INVALID if
 * the inputs don't match any of the inputs
 */
static BoardRevision_t revision_lookup(TrinaryInput_t *inputs);

// Public function implementation

BoardRevision_t board_revision_get(void) {
    if(!_has_been_checked) {
        TrinaryInput_t inputs[BOARD_REV_PIN_COUNT];
        for(int i = 0; i < BOARD_REV_PIN_COUNT; ++i) {
            inputs[i] = read_input(&_rev_gpio[i]);
        }
        _revision = revision_lookup(inputs);
        _has_been_checked = true;    
    }
    return _revision;
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
    HAL_Delay(100);
    reading_low = HAL_GPIO_ReadPin(gpio->port, gpio->pin);

    init.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(gpio->port, &init);
    HAL_Delay(100);
    reading_high = HAL_GPIO_ReadPin(gpio->port, gpio->pin);
    HAL_GPIO_DeInit(gpio->port, gpio->pin);

    if(reading_low != reading_high) {
        return INPUT_FLOATING;
    }
    return (reading_low == GPIO_PIN_RESET) ? INPUT_PULLDOWN : INPUT_PULLUP;
}

static BoardRevision_t revision_lookup(TrinaryInput_t *inputs) {
    if(!inputs) {
        return BOARD_REV_INVALID;
    }
    for(int i = 0; i < BOARD_REV_INVALID; ++i) {
        if((inputs[0] == _revisions[i].pins[0]) &&
           (inputs[0] == _revisions[i].pins[0]) &&
           (inputs[0] == _revisions[i].pins[0])) {
            return _revisions[i].revision;
        }
    }
    return BOARD_REV_INVALID;
}
