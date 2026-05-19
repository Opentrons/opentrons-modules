#pragma GCC push_options
#pragma GCC optimize("O0")

#include "firmware/vent_hardware.h"

#include <stdint.h>

#include "main.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal_gpio.h"

DAC_HandleTypeDef hdac1;

static double clamp(double val, double min, double max) {
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

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

    /*Configure Input GPIO pins : PA6 */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pin = VENT_FAULT_GPIO_Pin;
    HAL_GPIO_Init(VENT_FAULT_GPIO_Port, &GPIO_InitStruct);
}

static void vent_hardware_dac_init(void) {
    __HAL_RCC_DAC1_CLK_ENABLE();
    DAC_ChannelConfTypeDef sConfig = {0};

    /* DAC Initialization */
    hdac1.Instance = DAC1;
    if (HAL_DAC_Init(&hdac1) != HAL_OK) {
        Error_Handler();
    }

    /** DAC channel OUT1 config
      (PA4 = DAC1_OUT1 on STM32G4)
    */
    sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
    sConfig.DAC_DMADoubleDataMode = DISABLE;
    sConfig.DAC_SignedFormat = DISABLE;
    sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }

    HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_8B_R, 0);
}

void hw_set_vent_state(VentState state) {
    bool open = (bool)state;
    HAL_GPIO_WritePin(VENT_IN_GPIO_Port, VENT_IN_GPIO_Pin,
                      open ? GPIO_PIN_SET : GPIO_PIN_RESET);
    // Puts stepper into low power mode
    HAL_GPIO_WritePin(nSLEEP_DRV_MCU_GPIO_Port, nSLEEP_DRV_MCU_GPIO_Pin,
                      open ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

VentState hw_get_vent_state() {
    bool open =
        HAL_GPIO_ReadPin(VENT_IN_GPIO_Port, VENT_IN_GPIO_Pin) == GPIO_PIN_SET;
    return (VentState)open;
}

bool hw_vent_fault_detected() {
    return HAL_GPIO_ReadPin(VENT_FAULT_GPIO_Port, VENT_FAULT_GPIO_Pin);
}

void hw_set_vent_voltage(double volt) {
    volt = clamp(volt, 0.0, REF_VOLTAGE);
    uint32_t val = (uint32_t)(volt * DAC_FULLRANGE / REF_VOLTAGE);
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_8B_R, val);
}

void vent_hardware_init(void) {
    vent_hardware_gpio_init();
    vent_hardware_dac_init();
    // close the vent
    hw_set_vent_voltage(VENT_RUN_VOLT);
    hw_set_vent_state(CLOSED);
}
#pragma GCC pop_options
