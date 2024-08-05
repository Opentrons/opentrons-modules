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


void motor_hardware_init(void){

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
}

void hw_enable_motor(MotorID motor_id) {
    /*
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
    * */
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
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

