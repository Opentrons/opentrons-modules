#include "stm32f3xx_hal.h"
#include "parameters_conversion.h"
#include "motor_hardware.h"
#include "mc_tuning.h"
#include "mc_interface.h"
#include "mc_tasks.h"
#include "mc_config.h"
#include <string.h>  // for memset
#include <math.h> // for fabs

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


static void Error_Handler();

motor_hardware_handles *MOTOR_HW_HANDLE = NULL;

static void MX_NVIC_Init(void)
{
  /* TIM1_BRK_TIM15_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, 9, 0);
  HAL_NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);
  /* TIM1_UP_TIM16_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM1_UP_TIM16_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
  /* ADC1_2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(ADC1_2_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
  /* TIM2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM2_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(ADC_HandleTypeDef* adc1)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_InjectionConfTypeDef sConfigInjected = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  adc1->Instance = ADC1;
  adc1->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
  adc1->Init.Resolution = ADC_RESOLUTION_12B;
  adc1->Init.ScanConvMode = ADC_SCAN_ENABLE;
  adc1->Init.ContinuousConvMode = DISABLE;
  adc1->Init.DiscontinuousConvMode = DISABLE;
  adc1->Init.DataAlign = ADC_DATAALIGN_LEFT;
  adc1->Init.NbrOfConversion = 1;
  adc1->Init.DMAContinuousRequests = DISABLE;
  adc1->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc1->Init.LowPowerAutoWait = DISABLE;
  adc1->Init.Overrun = ADC_OVR_DATA_PRESERVED;
  if (HAL_ADC_Init(adc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(adc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_2;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
  sConfigInjected.InjectedSingleDiff = ADC_SINGLE_ENDED;
  sConfigInjected.InjectedNbrOfConversion = 2;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_19CYCLES_5;
  sConfigInjected.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
  sConfigInjected.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJECCONV_T1_TRGO;
  sConfigInjected.AutoInjectedConv = DISABLE;
  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
  sConfigInjected.QueueInjectedContext = ENABLE;
  sConfigInjected.InjectedOffset = 0;
  sConfigInjected.InjectedOffsetNumber = ADC_OFFSET_NONE;
  if (HAL_ADCEx_InjectedConfigChannel(adc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_14;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_2;
  if (HAL_ADCEx_InjectedConfigChannel(adc1, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(ADC_HandleTypeDef* adc2)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_InjectionConfTypeDef sConfigInjected = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Common config
  */
  adc2->Instance = ADC2;
  adc2->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
  adc2->Init.Resolution = ADC_RESOLUTION_12B;
  adc2->Init.ScanConvMode = ADC_SCAN_ENABLE;
  adc2->Init.ContinuousConvMode = DISABLE;
  adc2->Init.DiscontinuousConvMode = DISABLE;
  adc2->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  adc2->Init.ExternalTrigConv = ADC_SOFTWARE_START;
  adc2->Init.DataAlign = ADC_DATAALIGN_LEFT;
  adc2->Init.NbrOfConversion = 2;
  adc2->Init.DMAContinuousRequests = DISABLE;
  adc2->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  adc2->Init.LowPowerAutoWait = DISABLE;
  adc2->Init.Overrun = ADC_OVR_DATA_PRESERVED;
  if (HAL_ADC_Init(adc2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_14;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
  sConfigInjected.InjectedSingleDiff = ADC_SINGLE_ENDED;
  sConfigInjected.InjectedNbrOfConversion = 2;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_19CYCLES_5;
  sConfigInjected.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
  sConfigInjected.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJECCONV_T1_TRGO;
  sConfigInjected.AutoInjectedConv = DISABLE;
  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
  sConfigInjected.QueueInjectedContext = ENABLE;
  sConfigInjected.InjectedOffset = 0;
  sConfigInjected.InjectedOffsetNumber = ADC_OFFSET_NONE;
  if (HAL_ADCEx_InjectedConfigChannel(adc2, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_4;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_2;
  if (HAL_ADCEx_InjectedConfigChannel(adc2, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_11;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.SamplingTime = ADC_SAMPLETIME_7CYCLES_5;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(adc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(adc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(TIM_HandleTypeDef* tim1)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  tim1->Instance = TIM1;
  tim1->Init.Prescaler = ((TIM_CLOCK_DIVIDER) - 1);
  tim1->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
  tim1->Init.Period = ((PWM_PERIOD_CYCLES) / 2);
  tim1->Init.ClockDivision = TIM_CLOCKDIVISION_DIV2;
  tim1->Init.RepetitionCounter = (REP_COUNTER);
  tim1->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(tim1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(tim1) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_TRIGGER;
  sSlaveConfig.InputTrigger = TIM_TS_ITR1;
  if (HAL_TIM_SlaveConfigSynchro(tim1, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC4REF;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(tim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = ((PWM_PERIOD_CYCLES) / 4);
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(tim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(tim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(tim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM2;
  sConfigOC.Pulse = (((PWM_PERIOD_CYCLES) / 2) - (HTMIN));
  if (HAL_TIM_PWM_ConfigChannel(tim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_1;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_ENABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_LOW;
  sBreakDeadTimeConfig.Break2Filter = 3;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(tim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(tim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(TIM_HandleTypeDef* tim2)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_HallSensor_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  tim2->Instance = TIM2;
  tim2->Init.Prescaler = 0;
  tim2->Init.CounterMode = TIM_COUNTERMODE_UP;
  tim2->Init.Period = M1_HALL_TIM_PERIOD;
  tim2->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  tim2->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(tim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(tim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = M1_HALL_IC_FILTER;
  sConfig.Commutation_Delay = 0;
  if (HAL_TIMEx_HallSensor_Init(tim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC2REF;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(tim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

static void PlateLockTIM_Init(TIM_HandleTypeDef* tim3) {
  __HAL_RCC_TIM3_CLK_ENABLE();
  tim3->Instance = PLATE_LOCK_TIM;
  tim3->Init.Prescaler = 0;
  tim3->Init.CounterMode = TIM_COUNTERMODE_UP;
  tim3->Init.Period = PLATE_LOCK_PWM_GRANULARITY;
  tim3->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  tim3->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  HAL_TIM_PWM_Init(tim3);

  motor_hardware_plate_lock_off(tim3); 
}

/**
  * @brief  Configures EXTI Line0 (connected to PE0 pin) in interrupt mode
  * @param  None
  * @retval None
  */
static void EXTI0_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOE clock */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  
  /* Configure plate lock engaged optical switch*/
  GPIO_InitStructure.Pin = PLATE_LOCK_ENGAGED_Pin;
  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLUP;
  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(PLATE_LOCK_Port, &GPIO_InitStructure);

  /* Enable and set EXTI0 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI0_IRQn, 10, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/**
  * @brief  Configures EXTI Line4 (connected to PE4 pin) in interrupt mode
  * @param  None
  * @retval None
  */
static void EXTI4_Config(void)
{
  GPIO_InitTypeDef   GPIO_InitStructure;

  /* Enable GPIOE clock */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  
  /* Configure plate lock released optical switch*/
  GPIO_InitStructure.Pin = PLATE_LOCK_RELEASED_Pin;
  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_PULLUP;
  GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(PLATE_LOCK_Port, &GPIO_InitStructure);

  /* Enable and set EXTI4 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI4_IRQn, 10, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();


  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(M1_PWM_EN_W_GPIO_Port,
                    M1_PWM_EN_U_Pin|M1_PWM_EN_V_Pin|M1_PWM_EN_W_Pin,
                    GPIO_PIN_RESET);

  /*Configure GPIO pins : M1_PWM_EN_U_Pin M1_PWM_EN_V_Pin M1_PWM_EN_W_Pin */
  GPIO_InitStruct.Pin = M1_PWM_EN_U_Pin|M1_PWM_EN_V_Pin|M1_PWM_EN_W_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(M1_PWM_EN_W_GPIO_Port, &GPIO_InitStruct);

  memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
  GPIO_InitStruct.Pin = DRIVER_NSLEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  HAL_GPIO_Init(DRIVER_NSLEEP_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(DRIVER_NSLEEP_Port, DRIVER_NSLEEP_Pin, GPIO_PIN_SET);
  GPIO_InitStruct.Pin = SOLENOID_1_Pin | SOLENOID_2_Pin;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SOLENOID_1_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(SOLENOID_1_Port, SOLENOID_1_Pin | SOLENOID_2_Pin, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = SOLENOID_VREF_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SOLENOID_VREF_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PLATE_LOCK_IN_1_Pin | PLATE_LOCK_IN_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(PLATE_LOCK_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = PLATE_LOCK_NSLEEP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PLATE_LOCK_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(PLATE_LOCK_Port, PLATE_LOCK_NSLEEP_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = PLATE_LOCK_NFAULT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PLATE_LOCK_Port, &GPIO_InitStruct);
}

static void DAC_Init(DAC_HandleTypeDef* dac) {
  __HAL_RCC_DAC1_CLK_ENABLE();
  dac->Instance = DAC1;
  if (HAL_OK != HAL_DAC_Init(dac)) {
    Error_Handler();
  }
  DAC_ChannelConfTypeDef chan_config = {
     .DAC_Trigger = DAC_TRIGGER_NONE,
     .DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE,
  };
  HAL_DAC_ConfigChannel(dac, &chan_config, SOLENOID_DAC_CHANNEL);
  HAL_DAC_Start(dac, SOLENOID_DAC_CHANNEL);
  HAL_DAC_SetValue(dac, SOLENOID_DAC_CHANNEL, DAC_ALIGN_8B_R, 0);
}

/**
  * Initializes the Global MSP.
  */
void Hal_MspInit(void)
{
  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* System interrupt init*/

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

static uint32_t HAL_RCC_ADC12_CLK_ENABLED=0;

/**
* @brief ADC MSP Initialization
* This function configures the hardware resources used in this example
* @param hadc: ADC handle pointer
* @retval None
*/
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hadc->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */
    /* Peripheral clock enable */
    HAL_RCC_ADC12_CLK_ENABLED++;
    if(HAL_RCC_ADC12_CLK_ENABLED==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA1     ------> ADC1_IN2
    PB11     ------> ADC1_IN14
    */
    GPIO_InitStruct.Pin = M1_CURR_AMPL_U_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(M1_CURR_AMPL_U_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = M1_CURR_AMPL_V_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(M1_CURR_AMPL_V_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
  else if(hadc->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspInit 0 */

  /* USER CODE END ADC2_MspInit 0 */
    /* Peripheral clock enable */
    HAL_RCC_ADC12_CLK_ENABLED++;
    if(HAL_RCC_ADC12_CLK_ENABLED==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**ADC2 GPIO Configuration
    PA7     ------> ADC2_IN4
    PC4     ------> ADC2_IN5
    PC5     ------> ADC2_IN11
    PB11     ------> ADC2_IN14
    */
    GPIO_InitStruct.Pin = M1_CURR_AMPL_W_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(M1_CURR_AMPL_W_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = /*M1_TEMPERATURE_Pin|*/M1_BUS_VOLTAGE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = M1_CURR_AMPL_V_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(M1_CURR_AMPL_V_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC2_MspInit 1 */

  /* USER CODE END ADC2_MspInit 1 */
  }

}

/**
* @brief ADC MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param hadc: ADC handle pointer
* @retval None
*/
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
  if(hadc->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC12_CLK_ENABLED--;
    if(HAL_RCC_ADC12_CLK_ENABLED==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /**ADC1 GPIO Configuration
    PA1     ------> ADC1_IN2
    PB11     ------> ADC1_IN14
    */
    HAL_GPIO_DeInit(M1_CURR_AMPL_U_GPIO_Port, M1_CURR_AMPL_U_Pin);

    HAL_GPIO_DeInit(M1_CURR_AMPL_V_GPIO_Port, M1_CURR_AMPL_V_Pin);

    /* ADC1 interrupt DeInit */
  /* USER CODE BEGIN ADC1:ADC1_2_IRQn disable */
    /**
    * Uncomment the line below to disable the "ADC1_2_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(ADC1_2_IRQn); */
  /* USER CODE END ADC1:ADC1_2_IRQn disable */

  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
  else if(hadc->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspDeInit 0 */

  /* USER CODE END ADC2_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC12_CLK_ENABLED--;
    if(HAL_RCC_ADC12_CLK_ENABLED==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /**ADC2 GPIO Configuration
    PA7     ------> ADC2_IN4
    PC4     ------> ADC2_IN5
    PC5     ------> ADC2_IN11
    PB11     ------> ADC2_IN14
    */
    HAL_GPIO_DeInit(M1_CURR_AMPL_W_GPIO_Port, M1_CURR_AMPL_W_Pin);

    HAL_GPIO_DeInit(GPIOC, /*M1_TEMPERATURE_Pin|*/M1_BUS_VOLTAGE_Pin);

    HAL_GPIO_DeInit(M1_CURR_AMPL_V_GPIO_Port, M1_CURR_AMPL_V_Pin);

    /* ADC2 interrupt DeInit */
  /* USER CODE BEGIN ADC2:ADC1_2_IRQn disable */
    /**
    * Uncomment the line below to disable the "ADC1_2_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(ADC1_2_IRQn); */
  /* USER CODE END ADC2:ADC1_2_IRQn disable */

  /* USER CODE BEGIN ADC2_MspDeInit 1 */

  /* USER CODE END ADC2_MspDeInit 1 */
  }

}

/**
* @brief TIM_Base MSP Initialization
* This function configures the hardware resources used in this example
* @param htim_base: TIM_Base handle pointer
* @retval None
*/
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(htim_base->Instance==TIM1)
  {
  /* USER CODE BEGIN TIM1_MspInit 0 */

  /* USER CODE END TIM1_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_TIM1_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PA11     ------> TIM1_BKIN2
    */
    GPIO_InitStruct.Pin = M1_OCP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF6_TIM1;
    HAL_GPIO_Init(M1_OCP_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN TIM1_MspInit 1 */

  /* USER CODE END TIM1_MspInit 1 */
  }
  else if(htim_base->Instance==TIM2)
  {
  /* USER CODE BEGIN TIM2_MspInit 0 */

  /* USER CODE END TIM2_MspInit 0 */
    /* Peripheral clock enable */
    __HAL_RCC_TIM2_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = M1_HALL_H3_Pin|M1_HALL_H2_Pin|M1_HALL_H1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM2;
    HAL_GPIO_Init(M1_HALL_H3_GPIO_Port, &GPIO_InitStruct);
  /* USER CODE BEGIN TIM2_MspInit 1 */

  /* USER CODE END TIM2_MspInit 1 */
  }

}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(htim->Instance==TIM1)
  {
  /* USER CODE BEGIN TIM1_MspPostInit 0 */

  /* USER CODE END TIM1_MspPostInit 0 */

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PA8     ------> TIM1_CH1
    PA9     ------> TIM1_CH2
    PA10     ------> TIM1_CH3
    */
    GPIO_InitStruct.Pin = M1_PWM_UH_Pin|M1_PWM_VH_Pin|M1_PWM_WH_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_TIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN TIM1_MspPostInit 1 */

  /* USER CODE END TIM1_MspPostInit 1 */
  }

}
/**
* @brief TIM_Base MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param htim_base: TIM_Base handle pointer
* @retval None
*/
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* htim_base)
{
  if(htim_base->Instance==TIM1)
  {
  /* USER CODE BEGIN TIM1_MspDeInit 0 */

  /* USER CODE END TIM1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM1_CLK_DISABLE();

    /**TIM1 GPIO Configuration
    PA8     ------> TIM1_CH1
    PA9     ------> TIM1_CH2
    PA10     ------> TIM1_CH3
    PA11     ------> TIM1_BKIN2
    */
    HAL_GPIO_DeInit(GPIOA, M1_PWM_UH_Pin|M1_PWM_VH_Pin|M1_PWM_WH_Pin|M1_OCP_Pin);

    /* TIM1 interrupt DeInit */
    HAL_NVIC_DisableIRQ(TIM1_BRK_TIM15_IRQn);
    HAL_NVIC_DisableIRQ(TIM1_UP_TIM16_IRQn);
  /* USER CODE BEGIN TIM1_MspDeInit 1 */

  /* USER CODE END TIM1_MspDeInit 1 */
  }
  else if(htim_base->Instance==TIM2)
  {
  /* USER CODE BEGIN TIM2_MspDeInit 0 */

  /* USER CODE END TIM2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_TIM2_CLK_DISABLE();

    /**TIM2 GPIO Configuration
    PB10     ------> TIM2_CH3
    PA15     ------> TIM2_CH1
    PB3     ------> TIM2_CH2
    */
    HAL_GPIO_DeInit(GPIOB, M1_HALL_H3_Pin|M1_HALL_H2_Pin);

    HAL_GPIO_DeInit(M1_HALL_H1_GPIO_Port, M1_HALL_H1_Pin);

    /* TIM2 interrupt DeInit */
    HAL_NVIC_DisableIRQ(TIM2_IRQn);
  /* USER CODE BEGIN TIM2_MspDeInit 1 */

  /* USER CODE END TIM2_MspDeInit 1 */
  }

}

void motor_hardware_setup(motor_hardware_handles* handles) {
  MOTOR_HW_HANDLE = handles;
  MX_GPIO_Init();
  MX_ADC1_Init(&handles->adc1);
  MX_ADC2_Init(&handles->adc2);
  MX_TIM1_Init(&handles->tim1);
  MX_TIM2_Init(&handles->tim2);
  DAC_Init(&handles->dac1);
  MCboot(handles->mci, handles->mct);
  PlateLockTIM_Init(&handles->tim3);
  EXTI0_Config();
  EXTI4_Config();
  /* Initialize interrupts */
  MX_NVIC_Init();
}

void motor_hardware_solenoid_drive(DAC_HandleTypeDef* dac1, uint8_t dacval) {
    HAL_DAC_SetValue(dac1, SOLENOID_DAC_CHANNEL, DAC_ALIGN_8B_R, dacval);
    HAL_GPIO_WritePin(SOLENOID_1_Port, SOLENOID_1_Pin, GPIO_PIN_SET);
}

void motor_hardware_solenoid_release(DAC_HandleTypeDef* dac1) {
  HAL_GPIO_WritePin(SOLENOID_1_Port, SOLENOID_1_Pin, GPIO_PIN_RESET);
  HAL_DAC_SetValue(dac1, SOLENOID_DAC_CHANNEL, DAC_ALIGN_8B_R, 0);
}

void motor_hardware_plate_lock_on(TIM_HandleTypeDef* tim3, float power) {
  float power_scale = fabs(power);
  if (power_scale > 1.f) {
    power_scale = 1.f;
  }
  TIM_OC_InitTypeDef chan_config = {
     .OCMode = TIM_OCMODE_PWM1,
     .Pulse = (uint16_t)(PLATE_LOCK_PWM_GRANULARITY * power_scale),
     .OCPolarity = TIM_OCPOLARITY_HIGH,
     .OCIdleState = TIM_OCIDLESTATE_RESET
};
  uint32_t active_channel = ((power < 0) ? PLATE_LOCK_IN_1_Chan : PLATE_LOCK_IN_2_Chan);
  uint32_t passive_channel = ((power < 0) ? PLATE_LOCK_IN_2_Chan : PLATE_LOCK_IN_1_Chan);
  HAL_TIM_PWM_Stop(tim3, active_channel);
  HAL_TIM_PWM_Stop(tim3, passive_channel);
  HAL_TIM_PWM_ConfigChannel(tim3, &chan_config, active_channel);
  chan_config.OCMode = TIM_OCMODE_FORCED_INACTIVE;
  HAL_TIM_PWM_ConfigChannel(tim3, &chan_config, passive_channel);
  HAL_TIM_GenerateEvent(tim3, TIM_EVENTSOURCE_UPDATE);
  HAL_TIM_PWM_Start(tim3, passive_channel);
  HAL_TIM_PWM_Start(tim3,  active_channel);
}

void motor_hardware_plate_lock_off(TIM_HandleTypeDef* tim3) {
  TIM_OC_InitTypeDef chan_config = {
     .OCMode = TIM_OCMODE_FORCED_INACTIVE,
     .Pulse = 0,
     .OCPolarity = TIM_OCPOLARITY_HIGH,
     .OCIdleState = TIM_OCIDLESTATE_RESET
};
  HAL_TIM_PWM_Stop(tim3, PLATE_LOCK_IN_1_Chan);
  HAL_TIM_PWM_Stop(tim3, PLATE_LOCK_IN_2_Chan);
  HAL_TIM_OC_ConfigChannel(tim3, &chan_config, PLATE_LOCK_IN_1_Chan);
  HAL_TIM_OC_ConfigChannel(tim3, &chan_config, PLATE_LOCK_IN_2_Chan);
  HAL_TIM_GenerateEvent(tim3, TIM_EVENTSOURCE_UPDATE);
  HAL_TIM_PWM_Start(tim3, PLATE_LOCK_IN_1_Chan);
  HAL_TIM_PWM_Start(tim3, PLATE_LOCK_IN_2_Chan);
}

void motor_hardware_plate_lock_brake(TIM_HandleTypeDef* tim3) {
  float power_scale = 1.f;
  TIM_OC_InitTypeDef chan_config = {
     .OCMode = TIM_OCMODE_PWM1,
     .Pulse = (uint16_t)(PLATE_LOCK_PWM_GRANULARITY * power_scale),
     .OCPolarity = TIM_OCPOLARITY_HIGH,
     .OCIdleState = TIM_OCIDLESTATE_RESET
};
  HAL_TIM_PWM_Stop(tim3, PLATE_LOCK_IN_1_Chan);
  HAL_TIM_PWM_Stop(tim3, PLATE_LOCK_IN_2_Chan);
  HAL_TIM_OC_ConfigChannel(tim3, &chan_config, PLATE_LOCK_IN_1_Chan);
  HAL_TIM_OC_ConfigChannel(tim3, &chan_config, PLATE_LOCK_IN_2_Chan);
  HAL_TIM_GenerateEvent(tim3, TIM_EVENTSOURCE_UPDATE);
  HAL_TIM_PWM_Start(tim3, PLATE_LOCK_IN_1_Chan);
  HAL_TIM_PWM_Start(tim3, PLATE_LOCK_IN_2_Chan);
}

/******************************************************************************/
/*                 STM32F3xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f3xx.s).                                               */
/******************************************************************************/

/**
  * @brief  This function handles External line 0 interrupt request.
  * @param  None
  * @retval None
  */
void EXTI0_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(PLATE_LOCK_ENGAGED_Pin);
}

/**
  * @brief  This function handles External line 4 interrupt request.
  * @param  None
  * @retval None
  */
void EXTI4_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(PLATE_LOCK_RELEASED_Pin);
}

/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  optical_switch_results results;
  if (GPIO_Pin == PLATE_LOCK_ENGAGED_Pin) {
    results.closed = true;
    results.open = false;
  }
  if (GPIO_Pin == PLATE_LOCK_RELEASED_Pin) {
    results.open = true;
    results.closed = false;
  }
  MOTOR_HW_HANDLE->plate_lock_complete(&results);
}

bool motor_hardware_plate_lock_sensor_read(uint16_t GPIO_Pin)
{
  if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(PLATE_LOCK_Port, GPIO_Pin)) {
    return true;
  } else {
    return false;
  }
}

void Error_Handler() {
  while (true) {}
}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
