#include "firmware/ui_hardware.h"

#include <stdbool.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"

/** Local defines */

#define HEARTBEAT_LED_PORT (GPIOA)
#define HEARTBEAT_LED_PIN  (GPIO_PIN_3)

/** Hardware static data struct */

typedef struct {
    bool initialized;
} UIHardware_t;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static UIHardware_t ui_hardware = {
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
    //NOLINTNEXTLINE(performance-no-int-to-ptr)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    //NOLINTNEXTLINE(performance-no-int-to-ptr)
    HAL_GPIO_Init(HEARTBEAT_LED_PORT, &init);
    ui_hardware.initialized = true;
}

/**
 * @brief Set the heartbeat LED on or off
 */
void ui_hardware_set_heartbeat_led(bool setting) {
    if(!ui_hardware.initialized) {
        ui_hardware_initialize();
    }
    //NOLINTNEXTLINE(performance-no-int-to-ptr)
    HAL_GPIO_WritePin(HEARTBEAT_LED_PORT, HEARTBEAT_LED_PIN,
        setting ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
