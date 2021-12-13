#include "motor_hardware.h"
#include <string.h>  // for memset
#include <math.h> // for fabs

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

motor_hardware_handles* MOTOR_HW_HANDLE = NULL;
TIM_OC_InitTypeDef htim2_oc;
TIM_HandleTypeDef htim2;

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* Enable GPIOx clocks */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
  GPIO_InitStruct.Pin = SOLENOID_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SOLENOID_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_RESET);

  //reset pin init
  GPIO_InitStruct.Pin = LID_STEPPER_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL; //correct?
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; //correct? Confirm based on timer freq
  HAL_GPIO_Init(LID_STEPPER_ENABLE_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_RESET_Pin, GPIO_PIN_SET); //enable
  //enable pin init
  GPIO_InitStruct.Pin = LID_STEPPER_ENABLE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL; //correct?
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; //correct? Confirm based on timer freq
  HAL_GPIO_Init(LID_STEPPER_ENABLE_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_ENABLE_Pin, GPIO_PIN_SET); //disable output at init
  //fault pin init
  GPIO_InitStruct.Pin = LID_STEPPER_FAULT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL; //correct?
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; //correct? Confirm based on timer freq
  HAL_GPIO_Init(LID_STEPPER_ENABLE_Port, &GPIO_InitStruct);
  //vref pin init
  GPIO_InitStruct.Pin = LID_STEPPER_VREF_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; //need?
  HAL_GPIO_Init(LID_STEPPER_CONTROL_Port, &GPIO_InitStruct);
  //lid stepper direction pin init
  GPIO_InitStruct.Pin = LID_STEPPER_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL; //correct?
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; //correct? Confirm based on timer freq
  HAL_GPIO_Init(LID_STEPPER_CONTROL_Port, &GPIO_InitStruct);
  //lid stepper step pin init
  GPIO_InitStruct.Pin = LID_STEPPER_STEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL; //correct?
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; //correct? Confirm based on timer freq
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(LID_STEPPER_CONTROL_Port, &GPIO_InitStruct);
}

void HAL_TIM_OC_MspInit(TIM_HandleTypeDef* htim) {
    if (htim == &htim2) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM2_CLK_ENABLE();

        /* TIM2 interrupt Init */
        HAL_NVIC_SetPriority(TIM2_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    }
}

static void MX_DAC_Init(DAC_HandleTypeDef* hdac) {
  __HAL_RCC_DAC1_CLK_ENABLE();
  hdac->Instance = DAC1;
  if (HAL_OK != HAL_DAC_Init(hdac)) {
    Error_Handler();
  }
  DAC_ChannelConfTypeDef chan_config = {
     .DAC_Trigger = DAC_TRIGGER_NONE,
     .DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE,
  };
  HAL_DAC_ConfigChannel(hdac, &chan_config, LID_STEPPER_VREF_CHANNEL);
  HAL_DAC_Start(hdac, LID_STEPPER_VREF_CHANNEL);
  HAL_DAC_SetValue(hdac, LID_STEPPER_VREF_CHANNEL, DAC_ALIGN_8B_R, 0);
}

void MX_OC_Init(TIM_HandleTypeDef* htim, TIM_OC_InitTypeDef* htim_oc) {
  /* Compute TIM2 clock */
  uint32_t uwTimClock = HAL_RCC_GetPCLK1Freq();
  /* Compute the prescaler value to have TIM2 counter clock equal to 1MHz */
  uint32_t uwPrescalerValue = (uint32_t) ((uwTimClock / 1000000U) - 1U);
  uint32_t uwPeriodValue = __HAL_TIM_CALC_PERIOD(uwTimClock, uwPrescalerValue, 64000); //64000Hz for 75rpm in toggle mode
  
  htim->Instance = TIM2;
  htim->Init.Prescaler = uwPrescalerValue;
  htim->Init.CounterMode = TIM_COUNTERMODE_UP;
  htim->Init.Period = uwPeriodValue;
  htim->Init.ClockDivision = 0;
  htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  htim->Channel = HAL_TIM_ACTIVE_CHANNEL_1;
  if (HAL_TIM_OC_Init(htim) != HAL_OK) {
    Error_Handler(); //or return false?
  }
  
  htim_oc->OCMode = TIM_OCMODE_TOGGLE;
  htim_oc->OCPolarity = TIM_OCPOLARITY_HIGH;
  htim_oc->OCFastMode = TIM_OCFAST_DISABLE;
  htim_oc->OCIdleState = TIM_OCIDLESTATE_RESET;
  if (HAL_TIM_OC_ConfigChannel(htim, htim_oc, LID_STEPPER_STEP_Channel) != HAL_OK) {
    Error_Handler(); //or return false?
  }
}

void motor_hardware_setup(motor_hardware_handles* handles) {
  MOTOR_HW_HANDLE = handles;
  MX_GPIO_Init();
  MX_DAC_Init(&handles->dac1);
  MX_OC_Init(&htim2, &htim2_oc);
}

//PA0/PA1/PB10/PB11 = TIM2CH1/2/3/4 (all GPIO_AF1_TIM2)
//control via increment_step in motor_task.hpp?
//handle end switch IT start and stop
void motor_hardware_lid_stepper_start(float angle) {
  MOTOR_HW_HANDLE->lid_stepper.step_count = 0;
  int32_t step_target_scalar = ((128 * 200) / 360) * (99.5);
  MOTOR_HW_HANDLE->lid_stepper.step_target = angle * step_target_scalar; //(angle) * ((microsteps/rev) * (gear ratio))

  //MX_OC_Init(); //will need to call this with alternate period once lid seal stepper implemented
  HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_ENABLE_Pin, GPIO_PIN_RESET); //enable output

  if (angle > 0) {
    HAL_GPIO_WritePin(LID_STEPPER_CONTROL_Port, LID_STEPPER_DIR_Pin, GPIO_PIN_SET);
  } else {
    HAL_GPIO_WritePin(LID_STEPPER_CONTROL_Port, LID_STEPPER_DIR_Pin, GPIO_PIN_RESET);
  }

  HAL_TIM_OC_Start_IT(&htim2, LID_STEPPER_STEP_Channel);
}

void motor_hardware_lid_stepper_stop() {
  HAL_TIM_OC_Stop_IT(&htim2, LID_STEPPER_STEP_Channel);
  HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_ENABLE_Pin, GPIO_PIN_SET); //disable output
}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2) {
    motor_hardware_increment_step();
  }
}

void motor_hardware_increment_step() {
  MOTOR_HW_HANDLE->lid_stepper.step_count++;
  if (MOTOR_HW_HANDLE->lid_stepper.step_count > (MOTOR_HW_HANDLE->lid_stepper.step_target - 1)) {
      motor_hardware_lid_stepper_stop();
      MOTOR_HW_HANDLE->lid_stepper_complete();
  }
}

void motor_hardware_lid_stepper_set_dac(uint8_t dacval) {
  HAL_DAC_SetValue(&MOTOR_HW_HANDLE->dac1, LID_STEPPER_VREF_CHANNEL, DAC_ALIGN_8B_R, dacval);
}

bool motor_hardware_lid_stepper_reset(void) {
  bool bStatus = false;
  HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_RESET_Pin, GPIO_PIN_RESET);
  //check that fault cleared
  //this will be called by a gcode, return false if unsuccessful
  HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_RESET_Pin, GPIO_PIN_SET);
  return bStatus;
}

void motor_hardware_solenoid_engage() { //engage to clear/unlock sliding locking plate
  //check to confirm lid closed before engaging solenoid
  HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_SET);
  //delay to ensure engaged
}

void motor_hardware_solenoid_release() {
  HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_RESET);
  //delay to ensure disengaged
}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
