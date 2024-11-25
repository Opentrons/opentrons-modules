#include "stm32g4xx_hal_conf.h"

#include "firmware/tof_sensor_hardware.h"
//#include "common/firmware/errors.h"

static I2C_HandleTypeDef hi2c2;

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    if (hi2c->Instance == I2C2) {
        /** Initializes the peripherals clocks */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
        PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**I2C2 GPIO Configuration
        PA8     ------> I2C2_SDA
        PA9     ------> I2C2_SCL
        */
        GPIO_InitStruct.Pin = EEPROM_I2C2_SDA_Pin|EEPOM_I2C2_SCL_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C2_CLK_ENABLE();
    } 
    else if(hi2c->Instance==I2C3)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        /**I2C3 GPIO Configuration
        PC8     ------> I2C3_SCL
        PC9     ------> I2C3_SDA
        */
        GPIO_InitStruct.Pin = TOF_I2C3_SCL_Pin|TOF_I2C3_SDA_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF8_I2C3;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* Peripheral clock enable */
        __HAL_RCC_I2C3_CLK_ENABLE();
    }

    HAL_NVIC_SetPriority(I2C2_EV_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
    HAL_NVIC_SetPriority(I2C2_ER_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
}

HAL_I2C_HANDLE MX_I2C2_Init() {
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x10C0ECFF;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
    /** Configure Analogue filter
     */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) !=
        HAL_OK) {
        Error_Handler();
    }
    /** Configure Digital filter
     */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
        Error_Handler();
    }
    return &hi2c2;
}

/**
 * @brief enable the eeprom write protect pin.
 */
void eeprom_write_protect_init(void) {
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /*Configure GPIO pin : PC10 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = EEPROM_WP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(EEPROM_WP_PORT, &GPIO_InitStruct);
}

/**
 * @brief enable writing to the eeprom.
 */
void enable_eeprom_write() {
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_RESET);
}

/**
 * @brief disable writing to the eeprom.
 */
void disable_eeprom_write() {
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN, GPIO_PIN_SET);
}

void i2c_setup(I2CHandlerStruct* i2c_handles) {
    HAL_I2C_HANDLE i2c2 = MX_I2C2_Init();
    i2c_handles->i2c2 = i2c2;
    eeprom_write_protect_init();

    // write protect the eeprom.
    disable_eeprom_write();
}

void I2C2_EV_IRQHandler(void)
{
    HAL_I2C_EV_IRQHandler(&hi2c2);
}

void I2C2_ER_IRQHandler(void)
{
    HAL_I2C_ER_IRQHandler(&hi2c2);
}
