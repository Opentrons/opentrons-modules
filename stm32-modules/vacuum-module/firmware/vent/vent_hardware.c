#include <stdint.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_def.h"

#include "firmware/vent_hardware.h"
#include "main.h"

/**
 * @brief Initialize the Vent GPIO pins
 */
void vent_hardware_gpio_init(void) {
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure Output GPIO pins : PA3, PA4, and PA5 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    GPIO_InitStruct.Pin = nSLEEP_DRV_MCU_GPIO_Pin;
    HAL_GPIO_Init(nSLEEP_DRV_MCU_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = VENT_IN_GPIO_Pin;
    HAL_GPIO_Init(VENT_IN_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = VENT_DAC_MCU_GPIO_Pin;
    HAL_GPIO_Init(VENT_DAC_MCU_GPIO_Port, &GPIO_InitStruct);

    /*Configure Input GPIO pins : PA6 */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pin = VENT_FAULT_GPIO_Pin;
    HAL_GPIO_Init(VENT_FAULT_GPIO_Port, &GPIO_InitStruct);
}

void hw_open_vent(bool open) {
    HAL_GPIO_WritePin(VENT_IN_GPIO_Port, VENT_IN_GPIO_Pin, open ? GPIO_PIN_RESET : GPIO_PIN_SET);
    // Puts stepper into low power mode
    HAL_GPIO_WritePin(nSLEEP_DRV_MCU_GPIO_Port, nSLEEP_DRV_MCU_GPIO_Pin, open ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool hw_vent_fault_detected() {
    return HAL_GPIO_ReadPin(VENT_FAULT_GPIO_Port, VENT_FAULT_GPIO_Pin);
}

void vent_hardware_init(void) {
    vent_hardware_gpio_init();
    // close the vent
    hw_open_vent(false);
}
