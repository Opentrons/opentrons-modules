#include "stm32g4xx_hal_conf.h"

#include "firmware/tof_sensor_hardware.h"
#include "stm32g4xx_hal_gpio.h"
#include "systemwide.h"
#include "main.h"

static I2C_HandleTypeDef hi2c3;

HAL_I2C_HANDLE MX_I2C3_Init() {
    hi2c3.Instance = I2C3;
    hi2c3.Init.Timing = 0x10C0ECFF;
    hi2c3.Init.OwnAddress1 = 0;
    hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c3.Init.OwnAddress2 = 0;
    hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c3) != HAL_OK) {
        Error_Handler();
    }
    /** Configure Analogue filter
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) !=
        HAL_OK) {
        Error_Handler();
    }
    /** Configure Digital filter
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK) {
        Error_Handler();
    }
    return &hi2c3;
}

/**
 * @brief enable the eeprom write protect pin.
 */
void eeprom_write_protect_init(void) {
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pin : PA10 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = EEPROM_WP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
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
    GPIO_InitStruct.Pin = TOF_EN_X_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
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
        HAL_GPIO_WritePin(TOF_EN_X_PORT, TOF_EN_X_PIN, enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
    } else if (sensor_id == TOF_Z) {
        HAL_GPIO_WritePin(TOF_EN_Z_PORT, TOF_EN_Z_PIN, enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
    } else {
        Error_Handler();
    }
}

void i2c_setup(I2CHandlerStruct* i2c_handles) {
    HAL_I2C_HANDLE i2c3 = MX_I2C3_Init();
    i2c_handles->i2c3 = i2c3;
    eeprom_write_protect_init();

    // write protect the eeprom.
    enable_eeprom_write(false);
}

void I2C3_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c3);
}

void I2C3_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c3);
}
