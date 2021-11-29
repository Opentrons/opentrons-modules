#include "stm32g4xx_hal.h"
#include "motor_hardware.h"
#include <string.h>  // for memset
#include <math.h> // for fabs

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

motor_hardware_handles *MOTOR_HW_HANDLE = NULL;

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* Enable GPIOE clock */
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
  GPIO_InitStruct.Pin = SOLENOID_Pin;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SOLENOID_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_RESET);
}

void motor_hardware_setup(motor_hardware_handles* handles) {
  MOTOR_HW_HANDLE = handles;
  MX_GPIO_Init();
}

void motor_hardware_solenoid_engage() {
  //check to confirm lid closed before engaging solenoid
  //HAL_DAC_SetValue(dac1, SOLENOID_DAC_CHANNEL, DAC_ALIGN_8B_R, dacval);
  HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_SET);
  //delay to ensure engaged
}

void motor_hardware_solenoid_release() {
  HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_RESET);
  //HAL_DAC_SetValue(dac1, SOLENOID_DAC_CHANNEL, DAC_ALIGN_8B_R, 0);
  //delay to ensure disengaged
}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
