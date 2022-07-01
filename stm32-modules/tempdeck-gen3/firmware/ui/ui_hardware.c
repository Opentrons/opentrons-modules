#include "firmware/ui_hardware.h"

#include <stdbool.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"

/** Local defines */

#define HEARTBEAT_LED_PORT (GPIOB)
#define HEARTBEAT_LED_PIN  (GPIO_PIN_12)

/** Hardware static data struct */

typedef struct {
    bool initialized;
} UIHardware_t;

static UIHardware_t _ui = {
    .initialized = false
};

void ui_hardware_initialize() {
    GPIO_InitTypeDef init = {
        .Pin = HEARTBEAT_LED_PIN,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_LOW,
        .Alternate = 0
    };
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_Init(HEARTBEAT_LED_PORT, &init);
    _ui.initialized = true;
}

/**
 * @brief Set the heartbeat LED on or off
 */
void ui_hardware_set_heartbeat_led(bool setting) {
    if(!_ui.initialized) {
        ui_hardware_initialize();
    }
    // Active high
    HAL_GPIO_WritePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN,
        setting ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
