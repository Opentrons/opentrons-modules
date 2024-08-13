#include "main.h"

#include <stdbool.h>

#include "stm32g4xx_hal.h"


#define nSTATUS_LED_Pin GPIO_PIN_10
#define nSTATUS_LED_GPIO_Port GPIOC

void ui_hardware_set_heartbeat_led(bool setting) {
    //NOLINTNEXTLINE(performance-no-int-to-ptr)
    HAL_GPIO_WritePin(nSTATUS_LED_GPIO_Port, nSTATUS_LED_Pin,
        setting ? GPIO_PIN_SET : GPIO_PIN_RESET);

//    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2,
//                      setting ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
