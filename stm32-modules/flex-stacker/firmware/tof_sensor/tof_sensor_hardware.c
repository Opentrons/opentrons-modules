#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_gpio.h"

#include "firmware/tof_sensor_hardware.h"
#include "systemwide.h"
#include "main.h"


/**
 * @brief enable the eeprom write protect pin.
 */
void eeprom_write_protect_init(void) {
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pin : PA10 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = EEPROM_WP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(EEPROM_WP_PORT, &GPIO_InitStruct);
}

/**
 * @brief enable the TOF X/Z write protect pin.
 */
void tof_write_protect_init(void) {
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
 * enable/disable writing to the eeprom.
 */
void enable_eeprom_write(bool enable) {
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
 * enable/disable writing to a tof sensor.
 */
void enable_tof_sensor_write(TOFSensorID sensor_id, bool enable) {
    if (sensor_id == TOF_X) {
        HAL_GPIO_WritePin(TOF_EN_X_PORT, TOF_EN_X_PIN, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    } else if (sensor_id == TOF_Z) {
        HAL_GPIO_WritePin(TOF_EN_Z_PORT, TOF_EN_Z_PIN, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}
