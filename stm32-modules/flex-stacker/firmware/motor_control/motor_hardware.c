
#include "stm32g4xx_hal.h"
#include "stm32g4xx_it.h"
#include "systemwide.h"

/******************* Motor Z *******************/
/** Motor hardware **/
#define Z_STEP_PIN (GPIO_PIN_2)
#define Z_STEP_PORT (GPIOC)
#define Z_DIR_PIN (GPIO_PIN_1)
#define Z_DIR_PORT (GPIOC)
#define Z_EN_PIN (GPIO_PIN_3)
#define Z_EN_PORT (GPIOA)
#define Z_N_BRAKE_PIN (GPIO_PIN_7)
#define Z_N_BRAKE_PORT (GPIOB)

/** Limit switches **/
/* Note: Photointerrupters limit switches */
#define Z_MINUS_LIMIT_PIN (GPIO_PIN_3)
#define Z_MINUS_LIMIT_PORT (GPIOC)
#define Z_PLUS_LIMIT_PIN (GPIO_PIN_0)
#define Z_PLUS_LIMIT_PORT (GPIOA)


/******************* Motor X *******************/

/** Motor hardware **/
#define X_STEP_PIN (GPIO_PIN_7)
#define X_STEP_PORT (GPIOA)
#define X_DIR_PIN (GPIO_PIN_6)
#define X_DIR_PORT (GPIOA)
#define X_EN_PIN (GPIO_PIN_4)
#define X_EN_PORT (GPIOA)
#define X_N_BRAKE_PIN (GPIO_PIN_9)
#define X_N_BRAKE_PORT (GPIOB)


/** Limit switches **/
/* Note: Photointerrupters limit switches */
#define X_MINUS_LIMIT_PIN (GPIO_PIN_1)
#define X_MINUS_LIMIT_PORT (GPIOA)
#define X_PLUS_LIMIT_PIN (GPIO_PIN_2)
#define X_PLUS_LIMIT_PORT (GPIOA)


/******************* Motor L *******************/

/** Motor hardware **/
#define L_STEP_PIN (GPIO_PIN_1)
#define L_STEP_PORT (GPIOB)
#define L_DIR_PIN (GPIO_PIN_0)
#define L_DIR_PORT (GPIOB)
#define L_EN_PIN (GPIO_PIN_5)
#define L_EN_PORT (GPIOC)


/** Limit switches **/
/* Note: Mechanical limit switches */
#define L_N_HELD_PIN (GPIO_PIN_5)
#define L_N_HELD_PORT (GPIOB)
#define L_N_RELEASED_PIN (GPIO_PIN_11)
#define L_N_RELEASED_PORT (GPIO_PIN_C)

/**************** COMMON ********************/
#define ESTOP_PIN GPIO_PIN_6
#define ESTOP_PORT GPIOB


// Frequency of the motor interrupt callbacks is 300kHz, providing some extra
// overhead over the velocities used by this application.
#define MOTOR_INTERRUPT_FREQ (300000)
/** Frequency of the driving clock is 170MHz.*/
#define TIM_APB_FREQ (170000000)
/** Preload for APB to give a 10MHz clock.*/
#define TIM_PRELOAD (16)
/** Calculated TIM period.*/
#define TIM_PERIOD (((TIM_APB_FREQ/(TIM_PRELOAD + 1)) / MOTOR_INTERRUPT_FREQ) - 1)

TIM_HandleTypeDef htim17;
TIM_HandleTypeDef htim20;
TIM_HandleTypeDef htim3;

void motor_hardware_gpio_init(void){

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */
    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, Z_DIR_PIN|L_N_HELD_PIN, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOA, Z_EN_PIN|X_EN_PIN|X_DIR_PIN, GPIO_PIN_RESET);

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOB, L_DIR_PIN, GPIO_PIN_RESET);

    /*Configure GPIO pins :  Z_MINUS_LIMIT_PIN */
    GPIO_InitStruct.Pin = Z_MINUS_LIMIT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pins : Z_DIR_PIN L_EN_PIN */
    GPIO_InitStruct.Pin = Z_DIR_PIN|L_EN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*Configure GPIO pins : Z_PLUS_LIMIT_PIN LIMIT_X__Pin LIMIT_X_A2_Pin */
    GPIO_InitStruct.Pin = Z_PLUS_LIMIT_PIN|X_MINUS_LIMIT_PIN|X_PLUS_LIMIT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : Z_EN_PIN X_EN_PIN X_DIR_PIN */
    GPIO_InitStruct.Pin = Z_EN_PIN|X_EN_PIN|X_DIR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : L_DIR_PIN Z_N_BRAKE_PIN X_N_BRAKE_PIN*/
    GPIO_InitStruct.Pin = L_DIR_PIN | Z_N_BRAKE_PIN | X_N_BRAKE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pins : L_N_HELD_PIN */
    GPIO_InitStruct.Pin = L_N_HELD_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);


    GPIO_InitStruct.Pin = ESTOP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(ESTOP_PORT, &GPIO_InitStruct);


    GPIO_InitStruct.Pin = X_STEP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(X_STEP_PORT, &GPIO_InitStruct);


    GPIO_InitStruct.Pin = Z_STEP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(Z_STEP_PORT, &GPIO_InitStruct);


    GPIO_InitStruct.Pin = L_STEP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(L_STEP_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(ESTOP_PORT, ESTOP_PIN, RESET);
}

// X motor timer
void MX_TIM17_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim17.Instance = TIM17;
    htim17.Init.Prescaler = TIM_PRELOAD;
    htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim17.Init.Period = TIM_PERIOD;
    htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim17) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim17, &sMasterConfig) !=
        HAL_OK) {
        Error_Handler();
    }
}
// Z motor timer
void MX_TIM20_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim20.Instance = TIM20;
    htim20.Init.Prescaler = TIM_PRELOAD;
    htim20.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim20.Init.Period = TIM_PERIOD;
    htim20.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim20) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim20, &sMasterConfig) !=
        HAL_OK) {
        Error_Handler();
    }
}
// L motor timer
void MX_TIM3_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = TIM_PRELOAD;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = TIM_PERIOD;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) !=
        HAL_OK) {
        Error_Handler();
    }
}

void motor_hardware_interrupt_init(void){
    MX_TIM17_Init();
    MX_TIM20_Init();
    MX_TIM3_Init();
}

void motor_hardware_init(void){
    motor_hardware_gpio_init();
    motor_hardware_interrupt_init();
}

void hw_enable_motor(MotorID motor_id) {
    void* port;
    uint16_t pin;
    HAL_StatusTypeDef     status = HAL_OK;
    switch (motor_id) {
        case MOTOR_Z:
            port = Z_EN_PORT;
            pin = Z_EN_PIN;
            status = HAL_TIM_Base_Start_IT(&htim20);
            if (status == HAL_OK)
            {
                HAL_NVIC_SetPriority(TIM20_UP_IRQn, 6, 0);
                HAL_NVIC_EnableIRQ(TIM20_UP_IRQn);
            }
            break;
        case MOTOR_X:
            port = X_EN_PORT;
            pin = X_EN_PIN;
            status = HAL_TIM_Base_Start_IT(&htim17);
            if (status == HAL_OK)
            {
                HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM17_IRQn, 6, 0);
                HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM17_IRQn);
            }
            break;
        case MOTOR_L:
            port = L_EN_PORT;
            pin = L_EN_PIN;
            status = HAL_TIM_Base_Start_IT(&htim3);
            if (status == HAL_OK)
            {
                HAL_NVIC_SetPriority(TIM3_IRQn, 6, 0);
                HAL_NVIC_EnableIRQ(TIM3_IRQn);
            }
            break;
        default:
            return;
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

void step_motor(MotorID motor_id) {
    void* port;
    uint16_t pin;
    switch (motor_id) {
        case MOTOR_Z:
            port = Z_STEP_PORT;
            pin = Z_STEP_PIN;
            break;
        case MOTOR_X:
            port = X_STEP_PORT;
            pin = X_STEP_PIN;
            break;
        case MOTOR_L:
            port = L_STEP_PORT;
            pin = L_STEP_PIN;
            break;
        default:
            return;
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}

void unstep_motor(MotorID motor_id) {
    void* port;
    uint16_t pin;
    switch (motor_id) {
        case MOTOR_Z:
            port = Z_STEP_PORT;
            pin = Z_STEP_PIN;
            break;
        case MOTOR_X:
            port = X_STEP_PORT;
            pin = X_STEP_PIN;
            break;
        case MOTOR_L:
            port = L_STEP_PORT;
            pin = L_STEP_PIN;
            break;
        default:
            return;
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

void TIM3_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim3);
}

void TIM20_UP_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim20);
}

void TIM1_TRG_COM_TIM17_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim17);
}
