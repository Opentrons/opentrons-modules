#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_gpio.h"

#include "firmware/tof_sensor_hardware.h"
#include "systemwide.h"
#include "main.h"


/**
 * @brief Initialize the TOF X/Z enable protect pins.
 */
void tof_enable_pin_init(void) {
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /*Configure GPIO pin : PC12 and PB4 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    GPIO_InitStruct.Pin = TOF_EN_X_PIN;
    HAL_GPIO_Init(TOF_EN_X_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TOF_EN_Z_PIN;
    HAL_GPIO_Init(TOF_EN_Z_PORT, &GPIO_InitStruct);
}


/**
 * enable/disable the TOF sensor.
 */
void hw_enable_tof_sensor(TOFSensorID sensor_id, bool enable) {
    if (sensor_id == TOF_X) {
        HAL_GPIO_WritePin(TOF_EN_X_PORT, TOF_EN_X_PIN, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    } else if (sensor_id == TOF_Z) {
        HAL_GPIO_WritePin(TOF_EN_Z_PORT, TOF_EN_Z_PIN, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

// Initialize the eeprom and tof enable pins
void tof_hardware_init(void) {
    tof_enable_pin_init();
    // Disable the tof sensors
    hw_enable_tof_sensor(TOF_X, false);
    hw_enable_tof_sensor(TOF_Z, false);
}
