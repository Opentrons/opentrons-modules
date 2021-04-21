/**
  ******************************************************************************
  * @file    r1_f30x_pwm_curr_fdbk.c
  * @author  Motor Control SDK Team, ST Microelectronics
  * @brief   This file provides firmware functions that implement the following features
  *          of the CCC component of the Motor Control SDK:
  *           + initializes MCU peripheral for 1 shunt topology and F3 family
  *           + performs PWM duty cycle computation and generation
  *           + performs current sensing
  *           +
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
#include "r1_f30x_pwm_curr_fdbk.h"
#include "pwm_common.h"
#include "mc_type.h"

/** @addtogroup MCSDK
  * @{
  */

/** @addtogroup pwm_curr_fdbk
  * @{
  */

/**
 * @defgroup r1_f30x_pwm_curr_fdbk R1 F30x PWM & Current Feedback
 *
 * @brief STM32F3, 1-Shunt PWM & Current Feedback implementation
 *
 * This component is used in applications based on an STM32F3 MCU
 * and using a single shunt resistor current sensing topology.
 *
 * @todo: TODO: complete documentation.
 * @{
 */

/* Constant values -----------------------------------------------------------*/
#define TIMxCCER_MASK_CH123              (TIM_CCER_CC1E|TIM_CCER_CC2E|TIM_CCER_CC3E|\
    TIM_CCER_CC1NE|TIM_CCER_CC2NE|TIM_CCER_CC3NE)
#define CC12_PRELOAD_ENABLE_MASK         (TIM_CCMR1_OC1PE|TIM_CCMR1_OC2PE)
#define CC3_PRELOAD_ENABLE_MASK          TIM_CCMR2_OC3PE
#define CC1_PRELOAD_DISABLE_MASK         ~TIM_CCMR1_OC1PE
#define CC2_PRELOAD_DISABLE_MASK         ~TIM_CCMR1_OC2PE
#define CC3_PRELOAD_DISABLE_MASK         ~TIM_CCMR2_OC3PE
#define TIMxCCR56_PRELOAD_DISABLE_MASK   ~(TIM_CCMR3_OC5PE|TIM_CCMR3_OC6PE)
#define TIMxCCR56_PRELOAD_ENABLE_MASK    (TIM_CCMR3_OC5PE|TIM_CCMR3_OC6PE)

/* boundary zone definition */
#define REGULAR         ((uint8_t)0u)
#define BOUNDARY_1      ((uint8_t)1u)  /* Two small, one big */
#define BOUNDARY_2      ((uint8_t)2u)  /* Two big, one small */
#define BOUNDARY_3      ((uint8_t)3u)  /* Three equal        */

#define INVERT_NONE 0u
#define INVERT_A 1u
#define INVERT_B 2u
#define INVERT_C 3u

#define SAMP_NO 0u
#define SAMP_IA 1u
#define SAMP_IB 2u
#define SAMP_IC 3u
#define SAMP_NIA 4u
#define SAMP_NIB 5u
#define SAMP_NIC 6u
#define SAMP_OLDA 7u
#define SAMP_OLDB 8u
#define SAMP_OLDC 9u

static const uint8_t REGULAR_SAMP_CUR1[6] = {SAMP_NIC, SAMP_NIC, SAMP_NIA, SAMP_NIA, SAMP_NIB, SAMP_NIB};
static const uint8_t REGULAR_SAMP_CUR2[6] = {SAMP_IA, SAMP_IB, SAMP_IB, SAMP_IC, SAMP_IC, SAMP_IA};
static const uint8_t BOUNDR1_SAMP_CUR2[6] = {SAMP_IB, SAMP_IB, SAMP_IC, SAMP_IC, SAMP_IA, SAMP_IA};
static const uint8_t BOUNDR2_SAMP_CUR1[6] = {SAMP_IA, SAMP_IB, SAMP_IB, SAMP_IC, SAMP_IC, SAMP_IA};
static const uint8_t BOUNDR2_SAMP_CUR2[6] = {SAMP_IC, SAMP_IA, SAMP_IA, SAMP_IB, SAMP_IB, SAMP_IC};

/* Private typedef -----------------------------------------------------------*/



/* Private function prototypes -----------------------------------------------*/
static void R1F30X_HFCurrentsCalibration( PWMC_Handle_t * pHdl, ab_t * pStator_Currents );
static void R1F30X_SetAOReferenceVoltage( uint32_t DAC_Channel, uint16_t hDACVref );
static void R1F30X_1ShuntMotorVarsInit( PWMC_Handle_t * pHdl );
static void R1F30X_1ShuntMotorVarsRestart( PWMC_Handle_t * pHdl );
static void R1F30X_TIMxInit( TIM_TypeDef * TIMx, PWMC_R1_F3_Handle_t * pHdl );

/**
 * @brief  It initializes TIMx, ADC, GPIO, DMA1 and NVIC for current reading
 *         in ICS configuration using STM32F103x High Density
 * @param pHandle: handler of the current instance of the PWM component
 * @retval none
 */
void R1F30X_Init( PWMC_R1_F3_Handle_t * pHandle )
{
  OPAMP_TypeDef * OPAMPx = pHandle->pParams_str->OPAMP_Selection;
  COMP_TypeDef * COMP_OCPx = pHandle->pParams_str->CompOCPSelection;
  COMP_TypeDef * COMP_OVPx = pHandle->pParams_str->CompOVPSelection;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;
  ADC_TypeDef * ADCx = pHandle->pParams_str->ADCx;

  R1F30X_1ShuntMotorVarsInit( &pHandle->_Super );

  /* disable IT and flags in case of LL driver usage
   * workaround for unwanted interrupt enabling done by LL driver */
  LL_ADC_DisableIT_EOC( ADCx );
  LL_ADC_ClearFlag_EOC( ADCx );
  LL_ADC_DisableIT_JEOC( ADCx );
  LL_ADC_ClearFlag_JEOC( ADCx );


  /* Enable TIM1 - TIM8 clock */
  if ( TIMx == TIM1 )
  {

    /* DMA Event related to TIM1 Channel 4 */
    /* DMA1 Channel4 configuration ----------------------------------------------*/
    LL_DMA_SetMemoryAddress( DMA1, LL_DMA_CHANNEL_4, ( uint32_t )pHandle->DmaBuff );
    LL_DMA_SetPeriphAddress( DMA1, LL_DMA_CHANNEL_4, ( uint32_t ) & ( TIM1->CCR1 ) );
    LL_DMA_SetDataLength( DMA1, LL_DMA_CHANNEL_4, 2 );

    /* ensable DMA1 Channel4 */
    LL_DMA_EnableChannel( DMA1, LL_DMA_CHANNEL_4 );
    pHandle->DistortionDMAy_Chx = DMA1_Channel4;

    /* DMA Event related to TIM1 update */
    /* DMA1 Channel5 configuration ----------------------------------------------*/
    LL_DMA_SetMemoryAddress( DMA1, LL_DMA_CHANNEL_5, ( uint32_t ) & ( pHandle->PreloadDisableActing ) );
    LL_DMA_SetPeriphAddress( DMA1, LL_DMA_CHANNEL_5, ( uint32_t )&TIM1->CCMR1 );
    LL_DMA_SetDataLength( DMA1, LL_DMA_CHANNEL_5, 1 );

    /* enable DMA1 Channel5 */
    LL_DMA_EnableChannel( DMA1, LL_DMA_CHANNEL_5 );
    pHandle->PreloadDMAy_Chx = DMA1_Channel5;
  }
#if (defined(TIM8) && defined(DMA2))
  else
  {
    /* DMA Event related to TIM8 Channel 4 */
    /* DMA2 Channel2 configuration ----------------------------------------------*/
    LL_DMA_SetMemoryAddress( DMA2, LL_DMA_CHANNEL_2, ( uint32_t )pHandle->DmaBuff );
    LL_DMA_SetPeriphAddress( DMA2, LL_DMA_CHANNEL_2, ( uint32_t )&TIMx->CCR1 );
    LL_DMA_SetDataLength( DMA2, LL_DMA_CHANNEL_2, 2 );
    /* enable DMA2 Channel2 */
    LL_DMA_EnableChannel( DMA2, LL_DMA_CHANNEL_2 );
    pHandle->DistortionDMAy_Chx = DMA2_Channel2;

    /* DMA Event related to TIM8 update */
    /* DMA2 Channel1 configuration ----------------------------------------------*/
    LL_DMA_SetMemoryAddress( DMA2, LL_DMA_CHANNEL_1, ( uint32_t )&pHandle->PreloadDisableActing );
    LL_DMA_SetPeriphAddress( DMA2, LL_DMA_CHANNEL_1, ( uint32_t )&TIMx->CCMR1 );
    LL_DMA_SetDataLength( DMA2, LL_DMA_CHANNEL_1, 1 );
    /* enable DMA2 Channel2 */
    LL_DMA_EnableChannel( DMA2, LL_DMA_CHANNEL_1 );
    pHandle->PreloadDMAy_Chx = DMA2_Channel1;
  }
#endif

  R1F30X_TIMxInit( TIMx, pHandle );

  if ( OPAMPx )
  {
    /* enable OPAMP */
    LL_OPAMP_Enable( OPAMPx );
    LL_OPAMP_Lock( OPAMPx );
  }

  /* Over current protection */
  if ( COMP_OCPx )
  {
    /* Inverting input*/
    if ( pHandle->pParams_str->CompOCPInvInput_MODE != EXT_MODE )
    {
      if ( LL_COMP_GetInputMinus( COMP_OCPx ) == LL_COMP_INPUT_MINUS_DAC1_CH1 )
      {
        R1F30X_SetAOReferenceVoltage( LL_DAC_CHANNEL_1, ( uint16_t )( pHandle->pParams_str->DAC_OCP_Threshold ) );
      }
#if defined(DAC_CHANNEL2_SUPPORT)
      else if ( LL_COMP_GetInputMinus( COMP_OCPx ) == LL_COMP_INPUT_MINUS_DAC1_CH2 )
      {
        R1F30X_SetAOReferenceVoltage( LL_DAC_CHANNEL_2, ( uint16_t )( pHandle->pParams_str->DAC_OCP_Threshold ) );
      }
#endif
      else
      {}
    }

    /* Wait to stabilize DAC voltage */
    volatile uint16_t waittime = 0u;
    for ( waittime = 0u; waittime < 1000u; waittime++ )
    {}

    /* enable comparator */
#if defined(COMP_CSR_COMPxHYST)
    LL_COMP_SetInputHysteresis( COMP_OCPx, LL_COMP_HYSTERESIS_LOW );
#endif
    LL_COMP_Enable( COMP_OCPx );
    LL_COMP_Lock( COMP_OCPx );
  }

  /* Over voltage protection */
  if ( COMP_OVPx )
  {
    /* Inverting input*/
    if ( pHandle->pParams_str->CompOCPInvInput_MODE != EXT_MODE )
    {
      if ( LL_COMP_GetInputMinus( COMP_OVPx ) == LL_COMP_INPUT_MINUS_DAC1_CH1 )
      {
        R1F30X_SetAOReferenceVoltage( LL_DAC_CHANNEL_1, ( uint16_t )( pHandle->pParams_str->DAC_OVP_Threshold ) );
      }
#if defined(DAC_CHANNEL2_SUPPORT)
      else if ( LL_COMP_GetInputMinus( COMP_OVPx ) == LL_COMP_INPUT_MINUS_DAC1_CH2 )
      {
        R1F30X_SetAOReferenceVoltage( LL_DAC_CHANNEL_2, ( uint16_t )( pHandle->pParams_str->DAC_OVP_Threshold ) );
      }
#endif
      else
      {}
    }

    /* Wait to stabilize DAC voltage */
    volatile uint16_t waittime = 0u;
    for ( waittime = 0u; waittime < 1000u; waittime++ )
    {}

    /* enable comparator */
#if defined(COMP_CSR_COMPxHYST)
    LL_COMP_SetInputHysteresis( COMP_OCPx, LL_COMP_HYSTERESIS_LOW );
#endif
    LL_COMP_Enable( COMP_OVPx );
    LL_COMP_Lock( COMP_OVPx );

  }

  if ( pHandle->pParams_str->TIMx == TIM1 )
  {
    /* TIM1 Counter Clock stopped when the core is halted */
    LL_DBGMCU_APB2_GRP1_FreezePeriph( LL_DBGMCU_APB2_GRP1_TIM1_STOP );
  }
#if defined(TIM8)
  else
  {
    /* TIM8 Counter Clock stopped when the core is halted */
    LL_DBGMCU_APB2_GRP1_FreezePeriph( LL_DBGMCU_APB2_GRP1_TIM8_STOP );
  }
#endif

  LL_ADC_EnableInternalRegulator( ADCx );
  volatile uint32_t wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US / 10UL) * (SystemCoreClock / (100000UL * 2UL)));      
  while(wait_loop_index != 0UL)
  {
    wait_loop_index--;
  }

  LL_ADC_StartCalibration( ADCx, LL_ADC_SINGLE_ENDED );
  while ( LL_ADC_IsCalibrationOnGoing( ADCx ) )
  {
  }
  /* ADC Enable (must be done after calibration) */
  /* ADC5-140924: Enabling the ADC by setting ADEN bit soon after polling ADCAL=0 
  * following a calibration phase, could have no effect on ADC 
  * within certain AHB/ADC clock ratio.
  */
  while (  LL_ADC_IsActiveFlag_ADRDY( ADCx ) == 0u)  
  { 
    LL_ADC_Enable(  ADCx );
  }

  /* Flushing JSQR queue of context by setting JADSTP = 1 (JQM)=1 */
  LL_ADC_INJ_StopConversion( ADCx );

  if ( pHandle->pParams_str->TIMx == TIM1 )
  {
    LL_ADC_INJ_ConfigQueueContext( ADCx,
                                   LL_ADC_INJ_TRIG_EXT_TIM1_TRGO2,
                                   LL_ADC_INJ_TRIG_EXT_RISING,
                                   LL_ADC_INJ_SEQ_SCAN_ENABLE_2RANKS,
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel),
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel),
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel),
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel));
  }
#if defined(TIM8)
  else
  {
    LL_ADC_INJ_ConfigQueueContext( ADCx,
                                   LL_ADC_INJ_TRIG_EXT_TIM8_TRGO2,
                                   LL_ADC_INJ_TRIG_EXT_RISING,
                                   LL_ADC_INJ_SEQ_SCAN_ENABLE_2RANKS,
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel),
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel),
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel),
                                   __LL_ADC_DECIMAL_NB_TO_CHANNEL(pHandle->pParams_str->IChannel));
  }
#endif

  /* store register value in the handle to be used later during SVPWM for re-init */
  pHandle->ADC_JSQR = ADCx->JSQR;
  
  /* Flushing JSQR queue of context by setting JADSTP = 1 (JQM)=1 */
  LL_ADC_INJ_StartConversion( ADCx );
  LL_ADC_INJ_StopConversion( ADCx );
  /* store register value in the handle to be used later during SVPWM for re-init */
  

  LL_ADC_INJ_SetQueueMode( ADCx, LL_ADC_INJ_QUEUE_2CONTEXTS_END_EMPTY );
  LL_ADC_INJ_SetSequencerDiscont( ADCx, LL_ADC_INJ_SEQ_DISCONT_1RANK );

  /* Clear the flags */
  pHandle->OverVoltageFlag = false;
  pHandle->OverCurrentFlag = false;

  pHandle->_Super.DTTest = 0u;

}

/**
  * @brief  It initializes TIMx peripheral for PWM generation
  * @param TIMx: Timer to be initialized
  * @param pHandle: handler of the current instance of the PWM component
  * @retval none
  */
void R1F30X_TIMxInit( TIM_TypeDef * TIMx, PWMC_R1_F3_Handle_t * pHandle )
{

  /* disable main TIM counter to ensure
   * a synchronous start by TIM2 trigger */
  LL_TIM_DisableCounter( TIMx );

  /* Enables the TIMx Preload on CC1 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH1 );
  /* Enables the TIMx Preload on CC2 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH2 );
  /* Enables the TIMx Preload on CC3 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH3 );
  /* Enables the TIMx Preload on CC4 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH4 );
  /* Enables the TIMx Preload on CC5 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH5 );
  /* Enables the TIMx Preload on CC6 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH6 );

  /* Always enable BKIN for safety feature */
  LL_TIM_ClearFlag_BRK( TIMx );

  if ( ( pHandle->pParams_str->BKIN2Mode ) != NONE )
  {
    LL_TIM_ClearFlag_BRK2( TIMx );
  }
  LL_TIM_EnableIT_BRK( TIMx );

  /* Prepare timer for synchronization */
  LL_TIM_GenerateEvent_UPDATE( TIMx );
  if ( pHandle->pParams_str->FreqRatio == 2u )
  {
    if ( pHandle->pParams_str->IsHigherFreqTim == HIGHER_FREQ )
    {
      if ( pHandle->pParams_str->RepetitionCounter == 3u )
      {
        /* Set TIMx repetition counter to 1 */
        LL_TIM_SetRepetitionCounter( TIMx, 1 );
        LL_TIM_GenerateEvent_UPDATE( TIMx );
        /* Repetition counter will be set to 3 at next Update */
        LL_TIM_SetRepetitionCounter( TIMx, 3 );
      }
    }
    LL_TIM_SetCounter( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod ) - 1u );
  }
  else /* bFreqRatio equal to 1 or 3 */
  {
    if ( pHandle->_Super.Motor == M1 )
    {
      if ( pHandle->pParams_str->RepetitionCounter == 3u )
      {
        /* Set TIMx repetition counter to 1 */
        LL_TIM_SetRepetitionCounter( TIMx, 1 );
        LL_TIM_GenerateEvent_UPDATE( TIMx );
        /* Repetition counter will be set to 3 at next Update */
        LL_TIM_SetRepetitionCounter( TIMx, 3 );
      }
      LL_TIM_SetCounter( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod ) - 1u );
    }
  }

  /* Enable PWM channel */
  LL_TIM_CC_EnableChannel( TIMx, TIMxCCER_MASK_CH123 );

  pHandle->PreloadDisableCC1 = TIMx->CCMR1 & CC1_PRELOAD_DISABLE_MASK;
  pHandle->PreloadDisableCC2 = TIMx->CCMR1 & CC2_PRELOAD_DISABLE_MASK;
  pHandle->PreloadDisableCC3 = TIMx->CCMR2 & CC3_PRELOAD_DISABLE_MASK;

}

/**
  * @brief  First initialization of the handler
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
void R1F30X_1ShuntMotorVarsInit( PWMC_Handle_t * pHdl )
{
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;

  /* Init motor vars */
  pHandle->Inverted_pwm_new = INVERT_NONE;
  pHandle->Flags &= ( ~STBD3 ); /*STBD3 cleared*/
  pHandle->Flags &= ( ~DSTEN ); /*DSTEN cleared*/

  pHandle->Half_PWMPeriod = ( ( pHandle->_Super.PWMperiod ) / 2u );

  /* After reset, value of DMA buffers for distortion*/
  pHandle->DmaBuff[0] =  pHandle->Half_PWMPeriod + 1u;
  pHandle->DmaBuff[1] =  pHandle->Half_PWMPeriod >> 1; /*dummy*/

  /* Default value of sampling points */
  pHandle->CntSmp1 = ( pHandle->Half_PWMPeriod >> 1 ) + ( pHandle->pParams_str->Tafter );
  pHandle->CntSmp2 = pHandle->Half_PWMPeriod - 1u;
}

/**
  * @brief  Re-initialization of of the handler after each motor start
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
void R1F30X_1ShuntMotorVarsRestart( PWMC_Handle_t * pHdl )
{
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;

  /* Default value of sampling points */
  pHandle->CntSmp1 = ( pHandle->Half_PWMPeriod >> 1 ) + ( pHandle->pParams_str->Tafter );
  pHandle->CntSmp2 = pHandle->Half_PWMPeriod - 1u;

  pHandle->Inverted_pwm_new = INVERT_NONE;
  pHandle->Flags &= ( ~STBD3 ); /*STBD3 cleared*/

  /* Set the default previous value of Phase A,B,C current */
  pHandle->CurrAOld = 0;
  pHandle->CurrBOld = 0;

  pHandle->DmaBuff[0] =  pHandle->Half_PWMPeriod + 1u;
  pHandle->DmaBuff[1] =  pHandle->Half_PWMPeriod >> 1; /*dummy*/

  pHandle->BrakeActionLock = false;
}

/**
  * @brief It stores into pHandle the offset voltage read onchannels when no
  * current is flowing into the motor
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
__weak void R1F30X_CurrentReadingCalibration( PWMC_Handle_t * pHdl )
{

  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;
  pHandle->PhaseOffset = 0u;
  pHandle->Index = 0u;

  /* Disable distortion*/
  pHandle->Flags &= ( ~DSTEN ); /*DSTEN cleared*/

  /* It forces inactive level on TIMx CHy and CHyN */
  TIMx->CCER &= ( ~TIMxCCER_MASK_CH123 );

  /* Offset calibration  */
  /* Change function to be executed in ADCx_ISR */
  pHandle->_Super.pFctGetPhaseCurrents = &R1F30X_HFCurrentsCalibration;

  R1F30X_SwitchOnPWM( &pHandle->_Super );

  /* Wait for NB_CONVERSIONS to be executed */
  waitForPolarizationEnd( TIMx,
  		                  &pHandle->_Super.SWerror,
  						  pHandle->pParams_str->RepetitionCounter,
  						  &pHandle->Index );

  R1F30X_SwitchOffPWM( &pHandle->_Super );

  pHandle->PhaseOffset >>= 4;

  /* Change back function to be executed in ADCx_ISR */
  pHandle->_Super.pFctGetPhaseCurrents = &R1F30X_GetPhaseCurrents;

  /* It re-enable drive of TIMx CHy and CHyN by TIMx CHyRef*/
  TIMx->CCER |= TIMxCCER_MASK_CH123;

  /* Enable distortion*/
  pHandle->Flags |= DSTEN; /*DSTEN set*/

  R1F30X_1ShuntMotorVarsRestart( &pHandle->_Super );
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM) || defined(__GNUC__)
__attribute__( ( section ( ".ccmram" ) ) )
#endif
#endif
/**
  * @brief  It computes and return latest converted motor phase currents motor
  * @param pHdl: handler of the current instance of the PWM component
  * @retval Ia and Ib current in Curr_Components format
  */
__weak void R1F30X_GetPhaseCurrents( PWMC_Handle_t * pHdl, ab_t * pStator_Currents )
{
  int32_t wAux;
  int16_t hCurrA = 0;
  int16_t hCurrB = 0;
  int16_t hCurrC = 0;
  uint8_t bCurrASamp = 0u;
  uint8_t bCurrBSamp = 0u;
  uint8_t bCurrCSamp = 0u;

  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;
  ADC_TypeDef * ADCx = pHandle->pParams_str->ADCx;

  TIMx->CCMR1 |= CC12_PRELOAD_ENABLE_MASK;
  TIMx->CCMR2 |= CC3_PRELOAD_ENABLE_MASK;

  /* Reset the update flag to indicate the start of FOC algorithm*/
  pHandle->UpdateFlagBuffer = false;   

  /* First sampling point */
  wAux = ( int32_t )( ADCx->JDR1 );

  wAux -= ( int32_t )( pHandle->PhaseOffset );

  /* Check saturation */
  if ( wAux > -INT16_MAX )
  {
    if ( wAux < INT16_MAX )
    {
    }
    else
    {
      wAux = INT16_MAX;
    }
  }
  else
  {
    wAux = -INT16_MAX;
  }

  switch ( pHandle->sampCur1 )
  {
    case SAMP_IA:
      hCurrA = ( int16_t )( wAux );
      bCurrASamp = 1u;
      break;
    case SAMP_IB:
      hCurrB = ( int16_t )( wAux );
      bCurrBSamp = 1u;
      break;
    case SAMP_IC:
      hCurrC = ( int16_t )( wAux );
      bCurrCSamp = 1u;
      break;
    case SAMP_NIA:
      wAux = -wAux;
      hCurrA = ( int16_t )( wAux );
      bCurrASamp = 1u;
      break;
    case SAMP_NIB:
      wAux = -wAux;
      hCurrB = ( int16_t )( wAux );
      bCurrBSamp = 1u;
      break;
    case SAMP_NIC:
      wAux = -wAux;
      hCurrC = ( int16_t )( wAux );
      bCurrCSamp = 1u;
      break;
    case SAMP_OLDA:
      hCurrA = pHandle->CurrAOld;
      bCurrASamp = 1u;
      break;
    case SAMP_OLDB:
      hCurrB = pHandle->CurrBOld;
      bCurrBSamp = 1u;
      break;
    default:
      break;
  }

  /* Second sampling point */
  wAux = ( int32_t )( ADCx->JDR2 );

  wAux -= ( int32_t )( pHandle->PhaseOffset );

  /* Check saturation */
  if ( wAux > -INT16_MAX )
  {
    if ( wAux < INT16_MAX )
    {
    }
    else
    {
      wAux = INT16_MAX;
    }
  }
  else
  {
    wAux = -INT16_MAX;
  }

  switch ( pHandle->sampCur2 )
  {
    case SAMP_IA:
      hCurrA = ( int16_t )( wAux );
      bCurrASamp = 1u;
      break;
    case SAMP_IB:
      hCurrB = ( int16_t )( wAux );
      bCurrBSamp = 1u;
      break;
    case SAMP_IC:
      hCurrC = ( int16_t )( wAux );
      bCurrCSamp = 1u;
      break;
    case SAMP_NIA:
      wAux = -wAux;
      hCurrA = ( int16_t )( wAux );
      bCurrASamp = 1u;
      break;
    case SAMP_NIB:
      wAux = -wAux;
      hCurrB = ( int16_t )( wAux );
      bCurrBSamp = 1u;
      break;
    case SAMP_NIC:
      wAux = -wAux;
      hCurrC = ( int16_t )( wAux );
      bCurrCSamp = 1u;
      break;
    default:
      break;
  }

  /* Computation of the third value */
  if ( bCurrASamp == 0u )
  {
    wAux = -( ( int32_t )( hCurrB ) ) - ( ( int32_t )( hCurrC ) );

    /* Check saturation */
    if ( wAux > -INT16_MAX )
    {
      if ( wAux < INT16_MAX )
      {
      }
      else
      {
        wAux = INT16_MAX;
      }
    }
    else
    {
      wAux = -INT16_MAX;
    }

    hCurrA = ( int16_t )wAux;
  }
  if ( bCurrBSamp == 0u )
  {
    wAux = -( ( int32_t )( hCurrA ) ) - ( ( int32_t )( hCurrC ) );

    /* Check saturation */
    if ( wAux > -INT16_MAX )
    {
      if ( wAux < INT16_MAX )
      {
      }
      else
      {
        wAux = INT16_MAX;
      }
    }
    else
    {
      wAux = -INT16_MAX;
    }

    hCurrB = ( int16_t )wAux;
  }
  if ( bCurrCSamp == 0u )
  {
    wAux = -( ( int32_t )( hCurrA ) ) - ( ( int32_t )( hCurrB ) );

    /* Check saturation */
    if ( wAux > -INT16_MAX )
    {
      if ( wAux < INT16_MAX )
      {
      }
      else
      {
        wAux = INT16_MAX;
      }
    }
    else
    {
      wAux = -INT16_MAX;
    }

    hCurrC = ( int16_t )wAux;
  }

  /* hCurrA, hCurrB, hCurrC values are the sampled values */
  pHandle->CurrAOld = hCurrA;
  pHandle->CurrBOld = hCurrB;

  pStator_Currents->a = hCurrA;
  pStator_Currents->b = hCurrB;

  pHandle->_Super.Ia = hCurrA;
  pHandle->_Super.Ib = hCurrB;
  pHandle->_Super.Ic = -hCurrA - hCurrB;
}

/**
  * @brief  Implementaion of PWMC_GetPhaseCurrents to be performed during
  *         calibration. It sum up injected conversion data into wPhaseCOffset
  *         to compute the offset introduced in the current feedback
  *         network. It is requied to proper configure ADC input before to enable
  *         the offset computation.
  * @param pHdl: handler of the current instance of the PWM component
  * @retval It always returns {0,0} in Curr_Components format
  */
static void R1F30X_HFCurrentsCalibration( PWMC_Handle_t * pHdl, ab_t * pStator_Currents )
{
  /* Derived class members container */
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;
  ADC_TypeDef * ADCx = pHandle->pParams_str->ADCx;

  /* Reset the update flag to indicate the start of FOC algorithm*/
  pHandle->UpdateFlagBuffer = false;  

  if ( pHandle->Index < NB_CONVERSIONS )
  {
    pHandle->PhaseOffset += ADCx->JDR2;
    pHandle->Index++;

    /* fill the queue*/
    /* Preload Disable */
    TIMx->CCMR3 &= TIMxCCR56_PRELOAD_DISABLE_MASK;
    LL_TIM_OC_SetCompareCH5( TIMx, 0 );
    LL_TIM_OC_SetCompareCH6( TIMx, 0 );
    /* Preload enable */
    TIMx->CCMR3 |= TIMxCCR56_PRELOAD_ENABLE_MASK;

    LL_TIM_OC_SetCompareCH5( TIMx, ( ( uint32_t )( pHandle->Half_PWMPeriod ) >> 1 ) + ( uint32_t )(
                               pHandle->pParams_str->Tafter ) );
    LL_TIM_OC_SetCompareCH6( TIMx, ( ( uint32_t )( pHandle->Half_PWMPeriod ) - 1u ) );
  }

  /* during offset calibration no current is flowing in the phases */
  pStator_Currents->a = 0;
  pStator_Currents->b = 0;

}

/**
  * @brief  It turns on low sides switches. This function is intended to be
  *         used for charging boot capacitors of driving section. It has to be
  *         called each motor start-up when using high voltage drivers
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
__weak void R1F30X_TurnOnLowSides( PWMC_Handle_t * pHdl )
{
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;

  pHandle->_Super.TurnOnLowSidesAction = true;
  /*Turn on the three low side switches */
  LL_TIM_OC_SetCompareCH1( TIMx, 0 );
  LL_TIM_OC_SetCompareCH2( TIMx, 0 );
  LL_TIM_OC_SetCompareCH3( TIMx, 0 );

  /* Clear Update Flag */
  LL_TIM_ClearFlag_UPDATE( TIMx );

  /* Wait until next update */
  while ( LL_TIM_IsActiveFlag_UPDATE( TIMx ) == RESET )
  {}

  /* Main PWM Output Enable */
  LL_TIM_EnableAllOutputs( TIMx );

  if ( ( pHandle->pParams_str->LowSideOutputs ) == ES_GPIO )
  {
    LL_GPIO_SetOutputPin( pHandle->pParams_str->pwm_en_u_port, pHandle->pParams_str->pwm_en_u_pin );
    LL_GPIO_SetOutputPin( pHandle->pParams_str->pwm_en_v_port, pHandle->pParams_str->pwm_en_v_pin );
    LL_GPIO_SetOutputPin( pHandle->pParams_str->pwm_en_w_port, pHandle->pParams_str->pwm_en_w_pin );
  }
  return;
}


/**
  * @brief  It enables PWM generation on the proper Timer peripheral acting on MOE
  *         bit
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
__weak void R1F30X_SwitchOnPWM( PWMC_Handle_t * pHdl )
{
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;
  ADC_TypeDef * ADCx = pHandle->pParams_str->ADCx;
  pHandle->_Super.TurnOnLowSidesAction = false;

  LL_TIM_DisableDMAReq_CC4( TIMx );
  LL_TIM_DisableDMAReq_UPDATE( TIMx );
  pHandle->PreloadDMAy_Chx->CCR &= ( uint16_t )( ~DMA_CCR_EN );
  pHandle->DistortionDMAy_Chx->CCR &= ( uint16_t )( ~DMA_CCR_EN );
  pHandle->DistortionDMAy_Chx->CNDTR = 2u;

  /* Enables the TIMx Preload on CC1 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH1 );
  /* Enables the TIMx Preload on CC2 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH2 );
  /* Enables the TIMx Preload on CC3 Register */
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH3 );

  /* TIM output trigger 2 for ADC */
  LL_TIM_SetTriggerOutput2( TIMx, LL_TIM_TRGO2_OC5_RISING_OC6_RISING );

  /* wait for a new PWM period */
  LL_TIM_ClearFlag_UPDATE( TIMx );
  while ( LL_TIM_IsActiveFlag_UPDATE( TIMx ) == RESET )
  {}
  LL_TIM_ClearFlag_UPDATE( TIMx );

  /* Set all duty to 50% */
  /* Set ch5 ch6 for triggering */
  /* Clear Update Flag */
  /* TIM ch4 DMA request enable */

  pHandle->DmaBuff[1] = pHandle->Half_PWMPeriod >> 1;
  LL_TIM_OC_SetCompareCH1( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod >> 1 ) );
  LL_TIM_OC_SetCompareCH2( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod >> 1 ) );
  LL_TIM_OC_SetCompareCH3( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod >> 1 ) );

  
  /* Main PWM Output Enable */
  LL_TIM_EnableAllOutputs( TIMx );

  if ( ( pHandle->pParams_str->LowSideOutputs ) == ES_GPIO )
  {
    if ( ( TIMx->CCER & TIMxCCER_MASK_CH123 ) != 0u )
    {
      LL_GPIO_SetOutputPin( pHandle->pParams_str->pwm_en_u_port, pHandle->pParams_str->pwm_en_u_pin );
      LL_GPIO_SetOutputPin( pHandle->pParams_str->pwm_en_v_port, pHandle->pParams_str->pwm_en_v_pin );
      LL_GPIO_SetOutputPin( pHandle->pParams_str->pwm_en_w_port, pHandle->pParams_str->pwm_en_w_pin );
    }
    else
    {
      /* It is executed during calibration phase the EN signal shall stay off */
      LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_u_port, pHandle->pParams_str->pwm_en_u_pin );
      LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_v_port, pHandle->pParams_str->pwm_en_v_pin );
      LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_w_port, pHandle->pParams_str->pwm_en_w_pin );
    }
  }
  
  /* wait for new PWM cycle */
  LL_TIM_ClearFlag_UPDATE( TIMx );  
  while ( LL_TIM_IsActiveFlag_UPDATE( TIMx ) == RESET )
  {}    
  LL_TIM_EnableIT_UPDATE(TIMx);  
  LL_TIM_ClearFlag_UPDATE( TIMx );  
  
  /* Re-enable ADC triggering ( wADC_JSQRvalue has been computed stored
   * during the init routine ) */
  ADCx->JSQR = pHandle->ADC_JSQR;

  /*start injected conversion */
  LL_ADC_INJ_StartConversion( ADCx );

  /* clear injected end of sequence conversions flag*/
  LL_ADC_ClearFlag_JEOS( ADCx );

  /* enable injected end of sequence conversions interrupt */
  LL_ADC_EnableIT_JEOS( ADCx );
  
    /* ADC trigger setting (CH5+CH6 => TRGO2) */
  LL_TIM_OC_DisablePreload( TIMx, LL_TIM_CHANNEL_CH5 );
  LL_TIM_OC_DisablePreload( TIMx, LL_TIM_CHANNEL_CH6 );
  LL_TIM_OC_SetCompareCH5( TIMx, ( ( ( uint32_t )( pHandle->Half_PWMPeriod >> 1 ) ) +
                                   ( uint32_t )pHandle->pParams_str->Tafter ) );
  LL_TIM_OC_SetCompareCH6( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod - 1u ) );
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH5 );
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH6 );
  
  /* enable DMA transfer */
  LL_TIM_EnableDMAReq_CC4( TIMx );
  pHandle->DistortionDMAy_Chx->CCR |= DMA_CCR_EN;
}


/**
  * @brief  It disables PWM generation on the proper Timer peripheral acting on
  *         MOE bit
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
__weak void R1F30X_SwitchOffPWM( PWMC_Handle_t * pHdl )
{
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;
  ADC_TypeDef * ADCx = pHandle->pParams_str->ADCx;

  LL_TIM_DisableIT_UPDATE(TIMx);

  pHandle->_Super.TurnOnLowSidesAction = false;

  /* Main PWM Output Disable */
  LL_TIM_DisableAllOutputs( TIMx );
  if ( pHandle->BrakeActionLock == true )
  {
  }
  else
  {
	if ( ( pHandle->pParams_str->LowSideOutputs ) == ES_GPIO )
	{
	  LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_u_port, pHandle->pParams_str->pwm_en_u_pin );
	  LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_v_port, pHandle->pParams_str->pwm_en_v_pin );
	  LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_w_port, pHandle->pParams_str->pwm_en_w_pin );
	}
  }

  /* wait for new PWM cycle to flush last HF task */
  while ( LL_TIM_IsActiveFlag_UPDATE( TIMx ) == RESET )
  {}    
  LL_TIM_ClearFlag_UPDATE( TIMx );  
  
  /* Flushing JSQR queue of context by setting JADSTP = 1 (JQM)=1 */
  LL_ADC_INJ_StopConversion( ADCx );

  /* disable injected end of sequence conversions interrupt */
  LL_ADC_DisableIT_JEOS( ADCx );
    
  /* clear injected end of sequence conversions flag*/
  LL_ADC_ClearFlag_JEOS( ADCx );
  
  /* Flushing JSQR queue of context by setting JADSTP = 1 (JQM)=1 */
  LL_ADC_INJ_StopConversion( ADCx );

  /* disable injected end of sequence conversions interrupt */
  LL_ADC_DisableIT_JEOS( ADCx );

  /* Disable ADC trigger (CH5+CH6 => TRGO2) */
  LL_TIM_OC_DisablePreload( TIMx, LL_TIM_CHANNEL_CH5 );
  LL_TIM_OC_DisablePreload( TIMx, LL_TIM_CHANNEL_CH6 );
  LL_TIM_OC_SetCompareCH5( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod + 1u ) );
  LL_TIM_OC_SetCompareCH6( TIMx, ( uint32_t )( pHandle->Half_PWMPeriod + 1u ) );
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH5 );
  LL_TIM_OC_EnablePreload( TIMx, LL_TIM_CHANNEL_CH6 );

  /* Disable TIMx DMA requests enable */
  LL_TIM_DisableDMAReq_CC4( TIMx );
  LL_TIM_DisableDMAReq_UPDATE( TIMx );

  /* Disable DMA channels*/
  pHandle->PreloadDMAy_Chx->CCR &= ( uint16_t )( ~DMA_CCR_EN );


  /* wait for a new PWM period to flush last HF task */
  LL_TIM_ClearFlag_UPDATE( TIMx );
  while ( LL_TIM_IsActiveFlag_UPDATE( TIMx ) == 0u )
  {}
  LL_TIM_ClearFlag_UPDATE( TIMx );

  return;
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM) || defined(__GNUC__)
__attribute__( ( section ( ".ccmram" ) ) )
#endif
#endif

/**
  * @brief  It contains the TIMx Update event interrupt
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
__weak void * R1F30X_TIMx_UP_IRQHandler( PWMC_R1_F3_Handle_t * pHandle )
{
  pHandle->UpdateFlagBuffer = true;  
  return &( pHandle->_Super.Motor );
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM) || defined(__GNUC__)
__attribute__( ( section ( ".ccmram" ) ) )
#endif
#endif
/**
  * @brief  It contains the TIMx Break2 event interrupt
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
__weak void * R1F30X_BRK2_IRQHandler( PWMC_R1_F3_Handle_t * pHandle )
{
  if ( pHandle->BrakeActionLock == false )
  {
    if ( ( pHandle->pParams_str->LowSideOutputs ) == ES_GPIO )
    {
      LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_u_port, pHandle->pParams_str->pwm_en_u_pin );
      LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_v_port, pHandle->pParams_str->pwm_en_v_pin );
      LL_GPIO_ResetOutputPin( pHandle->pParams_str->pwm_en_w_port, pHandle->pParams_str->pwm_en_w_pin );
    }
  }
  pHandle->OverCurrentFlag = true;

  return &( pHandle->_Super.Motor );
}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM) || defined(__GNUC__)
__attribute__( ( section ( ".ccmram" ) ) )
#endif
#endif
/**
  * @brief  It contains the TIMx Break1 event interrupt
  * @param pHdl: handler of the current instance of the PWM component
  * @retval none
  */
__weak void * R1F30X_BRK_IRQHandler( PWMC_R1_F3_Handle_t * pHandle )
{
  pHandle->pParams_str->TIMx->BDTR |= LL_TIM_OSSI_ENABLE;
  pHandle->OverVoltageFlag = true;
  pHandle->BrakeActionLock = true;

  return &( pHandle->_Super.Motor );
}

/**
  * @brief  It is used to check if an overcurrent occurred since last call.
  * @param  pHdl: handler of the current instance of the PWM component
  * @retval uint16_t It returns MC_BREAK_IN whether an overcurrent has been
  *                  detected since last method call, MC_NO_FAULTS otherwise.
  */
__weak uint16_t R1F30X_IsOverCurrentOccurred( PWMC_Handle_t * pHdl )
{
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;

  uint16_t retVal = MC_NO_FAULTS;

  if ( pHandle->OverVoltageFlag == true )
  {
    retVal = MC_OVER_VOLT;
    pHandle->OverVoltageFlag = false;
  }

  if ( pHandle->OverCurrentFlag == true )
  {
    retVal |= MC_BREAK_IN;
    pHandle->OverCurrentFlag = false;
  }

  return retVal;
}

/**
  * @brief  It is used to configure the analog output used for protection
  *         thresholds.
  * @param  DAC_Channel: the selected DAC channel.
  *          This parameter can be:
  *            @arg DAC_Channel_1: DAC Channel1 selected
  *            @arg DAC_Channel_2: DAC Channel2 selected
  * @param  hDACVref Value of DAC reference expressed as 16bit unsigned integer.
  *         Ex. 0 = 0V 65536 = VDD_DAC.
  * @retval none
  */
void R1F30X_SetAOReferenceVoltage( uint32_t DAC_Channel, uint16_t hDACVref )
{

  LL_DAC_ConvertData12LeftAligned( DAC1, DAC_Channel, hDACVref );
  LL_DAC_TrigSWConversion( DAC1, DAC_Channel );
  /* Enable DAC Channel */
  LL_DAC_Enable( DAC1, DAC_Channel );

}

#if defined (CCMRAM)
#if defined (__ICCARM__)
#pragma location = ".ccmram"
#elif defined (__CC_ARM) || defined(__GNUC__)
__attribute__( ( section ( ".ccmram" ) ) )
#endif
#endif

/**
  * @brief  Implementation of the single shunt algorithm to setup the
  *         TIM1 register and DMA buffers values for the next PWM period.
  * @param  pHandle related object of class CPWMC
  * @retval uint16_t It returns MC_FOC_DURATION if the TIMx update occurs
  *         before the end of FOC algorithm else returns MC_NO_ERROR
  */
__weak uint16_t R1F30X_CalcDutyCycles( PWMC_Handle_t * pHdl )
{
  PWMC_R1_F3_Handle_t * pHandle = ( PWMC_R1_F3_Handle_t * )pHdl;
  TIM_TypeDef * TIMx = pHandle->pParams_str->TIMx;

  int16_t hDeltaDuty_0;
  int16_t hDeltaDuty_1;
  register uint16_t lowDuty = pHandle->_Super.lowDuty;
  register uint16_t midDuty = pHandle->_Super.midDuty;
  register uint16_t highDuty = pHandle->_Super.highDuty;  
  uint8_t bSector;
  uint8_t bStatorFluxPos;
  uint16_t hAux;

   bSector = ( uint8_t )( pHandle->_Super.Sector );
  if ( ( pHandle->Flags & DSTEN ) != 0u )
  {
    /* Compute delta duty */
    hDeltaDuty_0 = ( int16_t )( midDuty ) - ( int16_t )( highDuty );
    hDeltaDuty_1 = ( int16_t )( lowDuty ) - ( int16_t )( midDuty );
    /* Check region */
    if ( ( uint16_t )hDeltaDuty_0 <= pHandle->pParams_str->TMin )
    {
      if ( ( uint16_t )hDeltaDuty_1 <= pHandle->pParams_str->TMin )
      {
        bStatorFluxPos = BOUNDARY_3;
      }
      else
      {
        bStatorFluxPos = BOUNDARY_2;
      }
    }
    else
    {
      if ( ( uint16_t )hDeltaDuty_1 > pHandle->pParams_str->TMin )
      {
        bStatorFluxPos = REGULAR;
      }
      else
      {
        bStatorFluxPos = BOUNDARY_1;
      }
    }

    if ( bStatorFluxPos == BOUNDARY_1 ) /* Adjust the lower */
    {
      switch ( bSector )
      {
        case SECTOR_5:
        case SECTOR_6:
          if ( pHandle->_Super.CntPhA - pHandle->pParams_str->CHTMin - highDuty > pHandle->pParams_str->TMin )
          {
            pHandle->Inverted_pwm_new = INVERT_A;
            pHandle->_Super.CntPhA -= pHandle->pParams_str->CHTMin;
            if ( pHandle->_Super.CntPhA < midDuty )
            {
              midDuty = pHandle->_Super.CntPhA;
            }
          }
          else
          {
            bStatorFluxPos = BOUNDARY_3;
            if ( ( pHandle->Flags & STBD3 ) == 0u )
            {
              pHandle->Inverted_pwm_new = INVERT_A;
              pHandle->_Super.CntPhA -= pHandle->pParams_str->CHTMin;
              pHandle->Flags |= STBD3;
            }
            else
            {
              pHandle->Inverted_pwm_new = INVERT_B;
              pHandle->_Super.CntPhB -= pHandle->pParams_str->CHTMin;
              pHandle->Flags &= ( ~STBD3 );
            }
          }
          break;
        case SECTOR_2:
        case SECTOR_1:
          if ( pHandle->_Super.CntPhB - pHandle->pParams_str->CHTMin - highDuty > pHandle->pParams_str->TMin )
          {
            pHandle->Inverted_pwm_new = INVERT_B;
            pHandle->_Super.CntPhB -= pHandle->pParams_str->CHTMin;
            if ( pHandle->_Super.CntPhB < midDuty )
            {
              midDuty = pHandle->_Super.CntPhB;
            }
          }
          else
          {
            bStatorFluxPos = BOUNDARY_3;
            if ( ( pHandle->Flags & STBD3 ) == 0u )
            {
              pHandle->Inverted_pwm_new = INVERT_A;
              pHandle->_Super.CntPhA -= pHandle->pParams_str->CHTMin;
              pHandle->Flags |= STBD3;
            }
            else
            {
              pHandle->Inverted_pwm_new = INVERT_B;
              pHandle->_Super.CntPhB -= pHandle->pParams_str->CHTMin;
              pHandle->Flags &= ( ~STBD3 );
            }
          }
          break;
        case SECTOR_4:
        case SECTOR_3:
          if ( pHandle->_Super.CntPhC - pHandle->pParams_str->CHTMin - highDuty > pHandle->pParams_str->TMin )
          {
            pHandle->Inverted_pwm_new = INVERT_C;
            pHandle->_Super.CntPhC -= pHandle->pParams_str->CHTMin;
            if ( pHandle->_Super.CntPhC < midDuty )
            {
              midDuty = pHandle->_Super.CntPhC;
            }
          }
          else
          {
            bStatorFluxPos = BOUNDARY_3;
            if ( ( pHandle->Flags & STBD3 ) == 0u )
            {
              pHandle->Inverted_pwm_new = INVERT_A;
              pHandle->_Super.CntPhA -= pHandle->pParams_str->CHTMin;
              pHandle->Flags |= STBD3;
            }
            else
            {
              pHandle->Inverted_pwm_new = INVERT_B;
              pHandle->_Super.CntPhB -= pHandle->pParams_str->CHTMin;
              pHandle->Flags &= ( ~STBD3 );
            }
          }
          break;
        default:
          break;
      }
    }
    else if ( bStatorFluxPos == BOUNDARY_2 ) /* Adjust the middler */
    {
      switch ( bSector )
      {
        case SECTOR_4:
        case SECTOR_5: /* Invert B */
          pHandle->Inverted_pwm_new = INVERT_B;
          pHandle->_Super.CntPhB -= pHandle->pParams_str->CHTMin;
          if ( pHandle->_Super.CntPhB > 0xEFFFu )
          {
            pHandle->_Super.CntPhB = 0u;
          }
          break;
        case SECTOR_2:
        case SECTOR_3: /* Invert A */
          pHandle->Inverted_pwm_new = INVERT_A;
          pHandle->_Super.CntPhA -= pHandle->pParams_str->CHTMin;
          if ( pHandle->_Super.CntPhA > 0xEFFFu )
          {
            pHandle->_Super.CntPhA = 0u;
          }
          break;
        case SECTOR_6:
        case SECTOR_1: /* Invert C */
          pHandle->Inverted_pwm_new = INVERT_C;
          pHandle->_Super.CntPhC -= pHandle->pParams_str->CHTMin;
          if ( pHandle->_Super.CntPhC > 0xEFFFu )
          {
            pHandle->_Super.CntPhC = 0u;
          }
          break;
        default:
          break;
      }
    }
    else if ( bStatorFluxPos == BOUNDARY_3 )
    {
      if ( ( pHandle->Flags & STBD3 ) == 0u )
      {
        pHandle->Inverted_pwm_new = INVERT_A;
        pHandle->_Super.CntPhA -= pHandle->pParams_str->CHTMin;
        pHandle->Flags |= STBD3;
      }
      else
      {
        pHandle->Inverted_pwm_new = INVERT_B;
        pHandle->_Super.CntPhB -= pHandle->pParams_str->CHTMin;
        pHandle->Flags &= ( ~STBD3 );
      }
    }
    else
    {
    }

    if ( bStatorFluxPos == REGULAR ) /* Regular zone */
    {
      /* First point */
      pHandle->CntSmp1 = midDuty - pHandle->pParams_str->Tbefore;

      /* Second point */
      pHandle->CntSmp2 = lowDuty - pHandle->pParams_str->Tbefore;
    }

    if ( bStatorFluxPos == BOUNDARY_1 ) /* Two small, one big */
    {
      /* First point */
      pHandle->CntSmp1 = midDuty - pHandle->pParams_str->Tbefore;

      /* Second point */
      pHandle->CntSmp2 = pHandle->Half_PWMPeriod - pHandle->pParams_str->HTMin + pHandle->pParams_str->TSample;
    }

    if ( bStatorFluxPos == BOUNDARY_2 ) /* Two big, one small */
    {
      /* First point */
      pHandle->CntSmp1 = lowDuty - pHandle->pParams_str->Tbefore;

      /* Second point */
      pHandle->CntSmp2 = pHandle->Half_PWMPeriod - pHandle->pParams_str->HTMin + pHandle->pParams_str->TSample;
    }

    if ( bStatorFluxPos == BOUNDARY_3 )
    {
      /* First point */
      pHandle->CntSmp1 = highDuty - pHandle->pParams_str->Tbefore; /* Dummy trigger */
      /* Second point */
      pHandle->CntSmp2 = pHandle->Half_PWMPeriod - pHandle->pParams_str->HTMin + pHandle->pParams_str->TSample;
    }
  }
  else
  {
    pHandle->Inverted_pwm_new = INVERT_NONE;
    bStatorFluxPos = REGULAR;
  }

  /* Update Timer Ch4 for active vector*/
  /* Update Timer Ch 5,6 for ADC triggering and books the queue*/
  TIMx->CCMR3 &= TIMxCCR56_PRELOAD_DISABLE_MASK;
  TIMx->CCR5 = 0x0u;
  TIMx->CCR6 = 0xFFFFu;
  TIMx->CCMR3 |= TIMxCCR56_PRELOAD_ENABLE_MASK;

  LL_TIM_OC_SetCompareCH5(TIMx, pHandle->CntSmp1);
  LL_TIM_OC_SetCompareCH6(TIMx, pHandle->CntSmp2);

  if ( bStatorFluxPos == REGULAR )
  {
    /*LL_TIM_SetTriggerOutput2(TIMx, LL_TIM_TRGO2_OC5_RISING_OC6_RISING); */
    TIMx->CR2 &= ( ( uint32_t )0xFFEFFFFFu );

    switch ( pHandle->Inverted_pwm_new )
    {
      case INVERT_A:
        pHandle->DmaBuff[1] = pHandle->_Super.CntPhA;

        break;
      case INVERT_B:
        pHandle->DmaBuff[1] = pHandle->_Super.CntPhB;

        break;
      case INVERT_C:
        pHandle->DmaBuff[1] = pHandle->_Super.CntPhC;

        break;
      default:
        break;
    }
  }
  else
  {
    /* disable DMA request update interrupt */
    LL_TIM_DisableDMAReq_UPDATE( TIMx );


    /* Set the DMA destinations */
    switch ( pHandle->Inverted_pwm_new )
    {
      case INVERT_A:
        pHandle->DistortionDMAy_Chx->CPAR = ( uint32_t )( &( TIMx->CCR1 ) );
        pHandle->PreloadDMAy_Chx->CPAR = ( uint32_t )( &( TIMx->CCMR1 ) );
        pHandle->DmaBuff[1] = pHandle->_Super.CntPhA;
        pHandle->PreloadDisableActing = pHandle->PreloadDisableCC1;
        break;

      case INVERT_B:
        pHandle->DistortionDMAy_Chx->CPAR = ( uint32_t )( &( TIMx->CCR2 ) );
        pHandle->PreloadDMAy_Chx->CPAR = ( uint32_t )( &( TIMx->CCMR1 ) );
        pHandle->DmaBuff[1] = pHandle->_Super.CntPhB;
        pHandle->PreloadDisableActing = pHandle->PreloadDisableCC2;
        break;

      case INVERT_C:
        pHandle->DistortionDMAy_Chx->CPAR = ( uint32_t )( &( TIMx->CCR3 ) );
        pHandle->PreloadDMAy_Chx->CPAR = ( uint32_t )( &( TIMx->CCMR2 ) );
        pHandle->DmaBuff[1] = pHandle->_Super.CntPhC;
        pHandle->PreloadDisableActing = pHandle->PreloadDisableCC3;
        break;

      default:
        break;
    }

    /*TIM_SelectOutputTrigger2(TIMx, TIM_TRGO2Source_OC5RefRising_OC6RefFalling); */
    TIMx->CR2 |= ( ( uint32_t )0x100000u );

    /*active vector*/
    pHandle->PreloadDMAy_Chx->CCR &= ( uint16_t )( ~DMA_CCR_EN );
    pHandle->PreloadDMAy_Chx->CNDTR = 1u;
    pHandle->PreloadDMAy_Chx->CCR |= DMA_CCR_EN;

    /* enable DMA request update interrupt */
    LL_TIM_EnableDMAReq_UPDATE( TIMx );
  }

  pHandle->pParams_str->ADCx->JSQR = pHandle->ADC_JSQR;

  /* Update Timer Ch 1,2,3 (These value are required before update event) */
  LL_TIM_OC_SetCompareCH1(TIMx, pHandle->_Super.CntPhA);
  LL_TIM_OC_SetCompareCH2(TIMx, pHandle->_Super.CntPhB);
  LL_TIM_OC_SetCompareCH3(TIMx, pHandle->_Super.CntPhC);
  
  /*End of FOC*/
  /*check software error*/
  if ( pHandle->UpdateFlagBuffer == true )
  {
    hAux = MC_FOC_DURATION;
  }
  else
  {
    hAux = MC_NO_ERROR;
  }
  if ( pHandle->_Super.SWerror == 1u )
  {
    hAux = MC_FOC_DURATION;
    pHandle->_Super.SWerror = 0u;
  }

  /* Set the current sampled */
  if ( bStatorFluxPos == REGULAR ) /* Regual zone */
  {
    pHandle->sampCur1 = REGULAR_SAMP_CUR1[bSector];
    pHandle->sampCur2 = REGULAR_SAMP_CUR2[bSector];
  }

  if ( bStatorFluxPos == BOUNDARY_1 ) /* Two small, one big */
  {
    pHandle->sampCur1 = REGULAR_SAMP_CUR1[bSector];
    pHandle->sampCur2 = BOUNDR1_SAMP_CUR2[bSector];
  }

  if ( bStatorFluxPos == BOUNDARY_2 ) /* Two big, one small */
  {
    pHandle->sampCur1 = BOUNDR2_SAMP_CUR1[bSector];
    pHandle->sampCur2 = BOUNDR2_SAMP_CUR2[bSector];
  }

  if ( bStatorFluxPos == BOUNDARY_3 )
  {
    if ( pHandle->Inverted_pwm_new == INVERT_A )
    {
      pHandle->sampCur1 = SAMP_OLDB;
      pHandle->sampCur2 = SAMP_IA;
    }
    if ( pHandle->Inverted_pwm_new == INVERT_B )
    {
      pHandle->sampCur1 = SAMP_OLDA;
      pHandle->sampCur2 = SAMP_IB;
    }
  }

  /* Limit for the Get Phase current (Second EOC Handler) */

  return ( hAux );
}

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

/************************ (C) COPYRIGHT 2019 STMicroelectronics *****END OF FILE****/
