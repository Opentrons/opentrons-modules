/**
  ******************************************************************************
  * @file    r1_f30x_pwm_curr_fdbk.h
  * @author  Motor Control SDK Team, ST Microelectronics
  * @brief   This file contains all definitions and functions prototypes for the
  *          r1_f30x_pwm_curr_fdbk component of the Motor Control SDK.
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
  * @ingroup r1_f30x_pwm_curr_fdbk
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef R1_F30X_PWMCURRFDBK_H
#define R1_F30X_PWMCURRFDBK_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Includes ------------------------------------------------------------------*/
#include "pwm_curr_fdbk.h"

/** @addtogroup MCSDK
  * @{
  */

/** @addtogroup pwm_curr_fdbk
  * @{
  */

/** @addtogroup r1_f30x_pwm_curr_fdbk
  * @{
  */

/* Exported constants --------------------------------------------------------*/

#define GPIO_NoRemap_TIM1 ((uint32_t)(0))
#define SHIFTED_TIMs      ((uint8_t) 1)
#define NO_SHIFTED_TIMs   ((uint8_t) 0)

#define NONE    ((uint8_t)(0x00))
#define EXT_MODE  ((uint8_t)(0x01))
#define INT_MODE  ((uint8_t)(0x02))
#define STBD3 0x0002u /*!< Flag to indicate which phase has been distorted
                           in boudary 3 zone (A or B)*/
#define DSTEN 0x0004u /*!< Flag to indicate if the distortion must be performed
                           or not (in case of charge of bootstrap capacitor phase
                           is not required)*/

/* Exported types ------------------------------------------------------------*/

/**
 * @brief  Paamters structure of the r1_f30x_pwm_curr_fdbk Component.
 *
 */
typedef struct
{
  /* HW IP involved -----------------------------*/
  ADC_TypeDef * ADCx;            /*!< First ADC peripheral to be used.*/
  TIM_TypeDef * TIMx;              /*!< timer used for PWM generation.*/
  OPAMP_TypeDef * OPAMP_Selection;
  COMP_TypeDef * CompOCPSelection; /*!< Internal comparator used for Phases protection.*/
  COMP_TypeDef * CompOVPSelection;  /*!< Internal comparator used for Over Voltage protection.*/
  GPIO_TypeDef * pwm_en_u_port;    /*!< Channel 1N (low side) GPIO output */
  GPIO_TypeDef * pwm_en_v_port;    /*!< Channel 2N (low side) GPIO output*/
  GPIO_TypeDef * pwm_en_w_port;    /*!< Channel 3N (low side)  GPIO output */
                                        
  uint16_t pwm_en_u_pin;                    /*!< Channel 1N (low side) GPIO output pin */
  uint16_t pwm_en_v_pin;                    /*!< Channel 2N (low side) GPIO output pin */
  uint16_t pwm_en_w_pin;                    /*!< Channel 3N (low side)  GPIO output pin */
  
  
 /* PWM generation parameters --------------------------------------------------*/

  uint16_t TMin;                    /* */
  uint16_t HTMin;                    /* */
  uint16_t CHTMin;                    /* */
  uint16_t Tbefore;                 /* */
  uint16_t Tafter;                  /* */
  uint16_t TSample;  

  /* DAC settings --------------------------------------------------------------*/
  uint16_t DAC_OCP_Threshold;        /*!< Value of analog reference expressed
                                           as 16bit unsigned integer.
                                           Ex. 0 = 0V 65536 = VDD_DAC.*/
  uint16_t DAC_OVP_Threshold;        /*!< Value of analog reference expressed
                                           as 16bit unsigned integer.
                                           Ex. 0 = 0V 65536 = VDD_DAC.*/   
  /* PWM Driving signals initialization ----------------------------------------*/
  LowSideOutputsFunction_t LowSideOutputs; /*!< Low side or enabling signals
                                                generation method are defined
                                                here.*/
  uint8_t  IChannel;                                                

  uint8_t  RepetitionCounter;         /*!< It expresses the number of PWM
                                            periods to be elapsed before compare
                                            registers are updated again. In
                                            particular:
                                            RepetitionCounter= (2* #PWM periods)-1*/
  /* Emergency input (BKIN2) signal initialization -----------------------------*/
  uint8_t BKIN2Mode;                 /*!< It defines the modality of emergency
                                           input 2. It must be any of the
                                           the following:
                                           NONE - feature disabled.
                                           INT_MODE - Internal comparator used
                                           as source of emergency event.
                                           EXT_MODE - External comparator used
                                           as source of emergency event.*/
 
  /* Internal COMP settings ----------------------------------------------------*/                                 
  uint8_t       CompOCPInvInput_MODE;    /*!< COMPx inverting input mode. It must be either
                                                equal to EXT_MODE or INT_MODE. */                            
  uint8_t       CompOVPInvInput_MODE;     /*!< COMPx inverting input mode. It must be either
                                                equal to EXT_MODE or INT_MODE. */
  
  /* Dual MC parameters --------------------------------------------------------*/
  uint8_t  FreqRatio;             /*!< It is used in case of dual MC to
                                        synchronize TIM1 and TIM8. It has
                                        effect only on the second instanced
                                        object and must be equal to the
                                        ratio between the two PWM frequencies
                                        (higher/lower). Supported values are
                                        1, 2 or 3 */
  uint8_t  IsHigherFreqTim;       /*!< When bFreqRatio is greather than 1
                                        this param is used to indicate if this
                                        instance is the one with the highest
                                        frequency. Allowed value are: HIGHER_FREQ
                                        or LOWER_FREQ */                                           

} R1_F30XParams_t;

/**
  * @brief  Handle structure of the r1_f30x_pwm_curr_fdbk Component
  */
typedef struct
{
  PWMC_Handle_t _Super;       /*!< Offset of current sensing network  */
  uint16_t DmaBuff[2];       /*!< Buffer used for PWM distortion points*/
  uint16_t CntSmp1;          /*!< First sampling point express in timer counts*/
  uint16_t CntSmp2;          /*!< Second sampling point express in timer counts*/
  uint8_t sampCur1;           /*!< Current sampled in the first sampling point*/
  uint8_t sampCur2;           /*!< Current sampled in the second sampling point*/
  int16_t CurrAOld;          /*!< Previous measured value of phase A current*/
  int16_t CurrBOld;          /*!< Previous measured value of phase B current*/
  uint8_t Inverted_pwm_new;  /*!< This value indicates the type of the current
                                   PWM period (Regular, Distort PHA, PHB or PHC)*/
  uint16_t Flags;            /*!< Flags
                                   STBD3: Flag to indicate which phase has been distorted
                                          in boudary 3 zone (A or B)
                                   DSTEN: Flag to indicate if the distortion must be
                                          performed or not (charge of bootstrap
                                          capacitor phase) */
  uint16_t RegConv;          /*!< Temporary variables used to store regular conversions*/
  uint32_t PhaseOffset;      /*!< Offset of Phase current sensing network  */
  uint8_t  Index;   /*!< Number of conversions performed during the
                                   calibration phase*/
  uint16_t Half_PWMPeriod;  /* Half PWM Period in timer clock counts */
  uint32_t ADC_JSQR;   /*!< Stores the value for JSQR register to select
                                 phase A motor current.*/
  uint32_t PreloadDisableActing; /*!< Preload disable to be applied.*/
  uint32_t PreloadDisableCC1; /*!< CCMR1 that disables the preload register
                                     of the channel to be distorted.*/
  uint32_t PreloadDisableCC2; /*!< CCMR1 that disables the preload register
                                     of the channel to be distorted.*/
  uint32_t PreloadDisableCC3; /*!< CCMR2 that disables the preload register
                                     of the channel to be distorted.*/
  DMA_Channel_TypeDef * PreloadDMAy_Chx; /*!< DMA resource used for disabling the preload register*/
  DMA_Channel_TypeDef * DistortionDMAy_Chx; /*!< DMA resource used for doing the distortion*/
  bool UpdateFlagBuffer;  /*!< buffered version of Timer update IT flag */     
  bool OverCurrentFlag;     /*!< This flag is set when an overcurrent occurs.*/
  bool OverVoltageFlag;     /*!< This flag is set when an overvoltage occurs.*/
  bool BrakeActionLock;     /*!< This flag is set to avoid that brake action is interrupted.*/

  R1_F30XParams_t const * pParams_str;

} PWMC_R1_F3_Handle_t;

/**
  * It performs the initialization of the MCU peripherals required for
  * the PWM generation and current sensing. this initialization is dedicated
  * to one shunt topology and F3 family
  */
void R1F30X_Init( PWMC_R1_F3_Handle_t * pHandle );

/**
  * It disables PWM generation on the proper Timer peripheral acting on
  * MOE bit
  */
void R1F30X_SwitchOffPWM( PWMC_Handle_t * pHdl );

/**
  * It enables PWM generation on the proper Timer peripheral acting on MOE
  * bit
  */
void R1F30X_SwitchOnPWM( PWMC_Handle_t * pHdl );

/**
  * It turns on low sides switches. This function is intended to be
  * used for charging boot capacitors of driving section. It has to be
  * called each motor start-up when using high voltage drivers
  */
void R1F30X_TurnOnLowSides( PWMC_Handle_t * pHdl );

/**
  * It computes and return latest converted motor phase currents motor
  */
void R1F30X_GetPhaseCurrents( PWMC_Handle_t * pHdl, ab_t * pStator_Currents );

/**
  * It contains the TIMx Update event interrupt
  */
void * R1F30X_TIMx_UP_IRQHandler( PWMC_R1_F3_Handle_t * pHdl );

/**
  * It contains the TIMx Break2 event interrupt
  */
void * R1F30X_BRK2_IRQHandler( PWMC_R1_F3_Handle_t * pHdl );

/**
  * It contains the TIMx Break1 event interrupt
  */
void * R1F30X_BRK_IRQHandler( PWMC_R1_F3_Handle_t * pHdl );

/**
  * It stores into pHandle the offset voltage read onchannels when no
  * current is flowing into the motor
  */
void R1F30X_CurrentReadingCalibration( PWMC_Handle_t * pHdl );

/**
  * Implementation of the single shunt algorithm to setup the
  * TIM1 register and DMA buffers values for the next PWM period.
  */
uint16_t R1F30X_CalcDutyCycles( PWMC_Handle_t * pHdl );

/**
  * Execute a regular conversion using ADCx.
  * The function is not re-entrant (can't executed twice at the same time)
  */
uint16_t R1F30X_ExecRegularConv( PWMC_Handle_t * pHdl, uint8_t bChannel );

/**
  * It sets the specified sampling time for the specified ADC channel
  * on ADC1. It must be called once for each channel utilized by user
  */
void R1F30X_ADC_SetSamplingTime( PWMC_Handle_t * pHdl, ADConv_t ADConv_struct );

/**
  * It is used to check if an overcurrent occurred since last call.
  */
uint16_t R1F30X_IsOverCurrentOccurred( PWMC_Handle_t * pHdl );

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif /* __cpluplus */

#endif /*__R1_F30X_PWMCURRFDBK_H*/

/******************* (C) COPYRIGHT 2019 STMicroelectronics *****END OF FILE****/
