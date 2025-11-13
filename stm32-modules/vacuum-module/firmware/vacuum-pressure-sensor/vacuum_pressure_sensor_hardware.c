#include <stdbool.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_def.h"
#include "systemwide.h"
#include "main.h"

void vacuum_pressure_sensor_hardware_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // abs sensor_a_eoc
    GPIO_InitStruct.Pin = SENSOR_A_EOC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_A_EOC_PORT, &GPIO_InitStruct);

    // abs sensor_a_reset
    GPIO_InitStruct.Pin = SENSOR_A_RESET_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_A_RESET_PORT, &GPIO_InitStruct);

    // abs sensor_b_eoc
    GPIO_InitStruct.Pin = SENSOR_B_EOC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_B_EOC_PORT, &GPIO_InitStruct);

    // abs sensor_b_reset
    GPIO_InitStruct.Pin = SENSOR_B_RESET_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SENSOR_B_RESET_PORT, &GPIO_InitStruct);

    // abs sensor_b_reset
    GPIO_InitStruct.Pin = SENSOR_B_RESET_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SENSOR_B_RESET_PORT, &GPIO_InitStruct);

    // atm pressure eoc
    GPIO_InitStruct.Pin = ATMOSPHERIC_EOC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ATMOSPHERIC_EOC_PORT, &GPIO_InitStruct);
}

bool sensor_hardware_read_eoc_pin(PressureSensorID sensor_id) {
	uint8_t pin_val = false;
    if (sensor_id == ABS_PRESSURE_A) {
	    pin_val = HAL_GPIO_ReadPin(SENSOR_A_EOC_PORT, SENSOR_A_EOC_PIN);
	} else if (sensor_id == ABS_PRESSURE_B) {
		pin_val = HAL_GPIO_ReadPin(SENSOR_B_EOC_PORT, SENSOR_B_EOC_PIN);
	} else if (sensor_id == ATM_PRESSURE) {
		pin_val = HAL_GPIO_ReadPin(ATMOSPHERIC_EOC_PORT, ATMOSPHERIC_EOC_PIN);
    }
    // TODO: check pull
    return pin_val == GPIO_PIN_SET;
}

void sensor_hardware_sensor_reset(PressureSensorID sensor_id) {
	if (sensor_id == ABS_PRESSURE_A) {
	    HAL_GPIO_WritePin(SENSOR_A_RESET_PORT, SENSOR_A_RESET_PIN, GPIO_PIN_RESET);
	} else if(sensor_id == ABS_PRESSURE_B) {
	    HAL_GPIO_WritePin(SENSOR_B_RESET_PORT, SENSOR_B_RESET_PIN, GPIO_PIN_RESET);
    } else {
        // No reset pin for ATM_PRESSURE
    }
}
