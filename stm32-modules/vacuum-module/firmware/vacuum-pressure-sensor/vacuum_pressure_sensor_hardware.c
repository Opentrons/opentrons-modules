#include "systemwide.h"

// reset, eoc
// sensor a, sensor b 

// TODO: call this in a FreeRTOS Task startup routine
void vacuum_pressure_sensor_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    // sensor_a_eoc
    GPIO_InitStruct.Pin = SENSOR_A_EOC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_A_EOC_PORT, &GPIO_InitStruct);

    // sensor_a_reset
    GPIO_InitStruct.Pin = SENSOR_A_RESET_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_A_RESET_PORT, &GPIO_InitStruct);

    // sensor_b_eoc
    GPIO_InitStruct.Pin = SENSOR_B_EOC_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SENSOR_B_EOC_PORT, &GPIO_InitStruct);

    // sensor_b_reset
    GPIO_InitStruct.Pin = SENSOR_B_RESET_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SENSOR_B_RESET_PORT, &GPIO_InitStruct);
}


bool sensor_hardware_read_eoc_pin(VacuumPressureSensorId sensor_id) {
	if (sensor_id == VacuumPressureSensorId::SensorA) {
	    uint8_t pin_val = HAL_GPIO_ReadPin(SENSOR_A_EOC_PORT, SENSOR_A_EOC_PIN);
	}
	else {
		uint8_t pin_val = HAL_GPIO_ReadPin(SENSOR_B_EOC_PORT, SENSOR_B_EOC_PIN);
	}
    return pin_val == GPIO_PIN_SET ? true : false;
}

void sensor_hardware_sensor_reset(VacuumPressureSensorId sensor_id) {
	if (sensor_id == VacuumPressureSensorId::SensorA) {
	    HAL_GPIO_WritePin(SENSOR_A_RESET_PORT, SENSOR_A_RESET_PIN, GPIO_PIN_RESET);
	}
	else {
	    HAL_GPIO_WritePin(SENSOR_B_RESET_PORT, SENSOR_B_RESET_PIN, GPIO_PIN_RESET);
	}
}