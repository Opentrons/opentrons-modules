
/**
  ******************************************************************************
  * @file    mc_parameters.c
  * @author  Motor Control SDK Team, ST Microelectronics
  * @brief   This file provides definitions of HW parameters specific to the
  *          configuration of the subsystem.
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "motor_hardware.h"
#include "parameters_conversion.h"

#include "r3_2_f30x_pwm_curr_fdbk.h"

#define FREQ_RATIO 1                /* Dummy value for single drive */
#define FREQ_RELATION HIGHEST_FREQ  /* Dummy value for single drive */

  /**
  * @brief  Current sensor parameters Motor 1 - three shunt - F30x - Shared Resources
  */
const R3_2_Params_t R3_2_ParamsM1 =
{
/* Dual MC parameters --------------------------------------------------------*/
  .FreqRatio       = FREQ_RATIO,
  .IsHigherFreqTim = FREQ_RELATION,

/* Current reading A/D Conversions initialization -----------------------------*/
  .ADCx_1           = ADC1,
  .ADCx_2           = ADC2,

  .ADCConfig1 = { MC_ADC_CHANNEL_14<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_2<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_2<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_2<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_2<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_14<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                },
  .ADCConfig2 = { MC_ADC_CHANNEL_4<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_4<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_4<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_14<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_14<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                , MC_ADC_CHANNEL_4<<ADC_JSQR_JSQ1_Pos | (LL_ADC_INJ_TRIG_EXT_TIM1_TRGO & ~ADC_INJ_TRIG_EXT_EDGE_DEFAULT)
                },
  .ADCDataReg1 = { &ADC1->JDR1
                 , &ADC1->JDR1
                 , &ADC1->JDR1
                 , &ADC1->JDR1
                 , &ADC1->JDR1
                 , &ADC1->JDR1
                 },
  .ADCDataReg2 =  { &ADC2->JDR1
                  , &ADC2->JDR1
                  , &ADC2->JDR1
                  , &ADC2->JDR1
                  , &ADC2->JDR1
                  , &ADC2->JDR1
                  },
/* PWM generation parameters --------------------------------------------------*/
  .RepetitionCounter = REP_COUNTER,
  .Tafter            = TW_AFTER,
  .Tbefore           = TW_BEFORE,
  .TIMx               = TIM1,

/* PWM Driving signals initialization ----------------------------------------*/
  .LowSideOutputs = (LowSideOutputsFunction_t)LOW_SIDE_SIGNALS_ENABLING,
 .pwm_en_u_port      = M1_PWM_EN_U_GPIO_Port,
 .pwm_en_u_pin       = M1_PWM_EN_U_Pin,
 .pwm_en_v_port      = M1_PWM_EN_V_GPIO_Port,
 .pwm_en_v_pin       = M1_PWM_EN_V_Pin,
 .pwm_en_w_port      = M1_PWM_EN_W_GPIO_Port,
 .pwm_en_w_pin       = M1_PWM_EN_W_Pin,

/* Emergency input (BKIN2) signal initialization -----------------------------*/
  .BKIN2Mode     = EXT_MODE,

/* Internal OPAMP common settings --------------------------------------------*/
  .OPAMPParams     = MC_NULL,
/* Internal COMP settings ----------------------------------------------------*/
  .CompOCPASelection     = MC_NULL,
  .CompOCPAInvInput_MODE = NONE,
  .CompOCPBSelection     = MC_NULL,
  .CompOCPBInvInput_MODE = NONE,
  .CompOCPCSelection     = MC_NULL,
  .CompOCPCInvInput_MODE = NONE,

  .CompOVPSelection      = MC_NULL,
  .CompOVPInvInput_MODE  = NONE,

/* DAC settings --------------------------------------------------------------*/
  .DAC_OCP_Threshold =  23830,
  .DAC_OVP_Threshold =  23830,
};

/******************* (C) COPYRIGHT 2019 STMicroelectronics *****END OF FILE****/
