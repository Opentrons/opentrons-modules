#include "firmware/thermal_fan_hardware.h"

#include <stdatomic.h>
#include <string.h>

#include "FreeRTOS.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"
#include "task.h"

// Private definitions
#define SINK_FAN_PWM_Pin (GPIO_PIN_6)
#define SINK_FAN_PWM_GPIO_Port (GPIOA)

#define PULSE_WIDTH_FREQ (1000)
#define TIMER_CLOCK_FREQ (170000000)
// These two together give a 25kHz pulse width, and the ARR value
// of 99 gives us a nice scale of 0-100 for the pulse width.
// A finer scale is possible by reducing the prescale value and
// adjusting the reload to match.
#define TIM16_PRESCALER (67)
//#define TIM1_RELOAD    (99)
#define TIM16_RELOAD ((TIMER_CLOCK_FREQ / (PULSE_WIDTH_FREQ * (TIM16_PRESCALER + 1))) - 1)
// PWM should be scaled from 0 to MAX_PWM, inclusive
#define MAX_PWM (TIM16_RELOAD + 1)

#define TACH_NUM_READINGS (3)
// Tachometer timer reload frequency
#define TACH_TIMER_FREQ (4)
#define TACH_TIMER_PRESCALE (1699)
#define TACH_TIMER_PRESCALED_FREQ (TIMER_CLOCK_FREQ / (TACH_TIMER_PRESCALE + 1))
#define SEC_PER_MIN (60)
#define PULSES_PER_ROTATION (2)

#define TACH_TIMER_RELOAD ((TIMER_CLOCK_FREQ / (TACH_TIMER_FREQ * (TACH_TIMER_PRESCALE + 1))) -1)

// Private typedefs

struct Tachometer {
    TIM_HandleTypeDef timer;
    DMA_HandleTypeDef tach_1_dma;
    DMA_HandleTypeDef tach_2_dma;
    _Atomic uint16_t buffer_1[TACH_NUM_READINGS];
    _Atomic uint16_t buffer_2[TACH_NUM_READINGS];
    _Atomic uint32_t tach_1_period;
    _Atomic uint32_t tach_2_period;
};

struct Fans {
    // Configuration
    GPIO_TypeDef *enable_port;
    const uint16_t enable_pin;
    const uint16_t pwm_channel;
    // Current status
    bool initialized;
    double power;
    TIM_HandleTypeDef timer;
    struct Tachometer tach;
};

// Local variables

static struct Fans _fans = {
    .enable_port = GPIOD,
    .enable_pin  = GPIO_PIN_1,
    .pwm_channel = TIM_CHANNEL_1,
    .initialized = false,
    .power = 0.0F,
    .timer = {},
    .tach = {
        .timer = {},
        .buffer_1 = {},
        .buffer_2 = {},
        .tach_1_period = 0,
        .tach_2_period = 0
    }
};

// Private function declarations

static void thermal_fan_init_tach(struct Tachometer *tach);
static void thermal_fan_restart_tach_dma();
static void thermal_fan_setup_tach_timer();

/**
 * @brief Enable or disable the fans (12v power supply)
 * 
 * @param[in] enabled True to enable the fans, false to turn them off
 * @return True on success, false if \p enabled could not be written
 */
static bool thermal_fan_set_enable(bool enabled);

// Public function implementations

void thermal_fan_initialize(void) {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __GPIOA_CLK_ENABLE();
    __GPIOD_CLK_ENABLE();

    // Disable the 12v converter first
    GPIO_InitStruct.Pin = _fans.enable_pin;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(_fans.enable_port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(_fans.enable_port, _fans.enable_pin, GPIO_PIN_RESET);

    // Configure timer 16 for PWMN control on channel 1
    _fans.timer.Instance = TIM16;
    _fans.timer.Init.Prescaler = TIM16_PRESCALER;
    _fans.timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    _fans.timer.Init.Period = TIM16_RELOAD;
    _fans.timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    _fans.timer.Init.RepetitionCounter = 0;
    _fans.timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_Base_Init(&_fans.timer);
    configASSERT(hal_ret == HAL_OK);
    hal_ret = HAL_TIM_PWM_Init(&_fans.timer);
    configASSERT(hal_ret == HAL_OK);
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    hal_ret = HAL_TIM_PWM_ConfigChannel(&_fans.timer, &sConfigOC, _fans.pwm_channel);
    configASSERT(hal_ret == HAL_OK);
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(&_fans.timer, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);

    // This is a copy+paste of the HAL_TIM_MspPostInit implementation because
    // there's no actual reason for it to live in that function
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM16 GPIO Configuration
    PA6     ------> TIM16_CH1
    */
    GPIO_InitStruct.Pin = SINK_FAN_PWM_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM16;
    HAL_GPIO_Init(SINK_FAN_PWM_GPIO_Port, &GPIO_InitStruct);

    // Configure timer 4 for Input Capture mode to read the tachometers
    thermal_fan_init_tach(&_fans.tach);

    thermal_fan_setup_tach_timer();
    thermal_fan_restart_tach_dma();

    _fans.initialized = true;
}


bool thermal_fan_set_power(double power) {
    if(!_fans.initialized) { return false; }
    if(power > 1.0F) { power = 1.0F; }
    if(power < 0.0F) { power = 0.0F; }

    double old_power = _fans.power;
    _fans.power = power;

    if(power == 0.0F) {
        if(!thermal_fan_set_enable(false)) { return false; }
        return HAL_TIM_PWM_Stop(&_fans.timer, _fans.pwm_channel) == HAL_OK;
    }

    uint32_t pwm = _fans.power * (double)MAX_PWM;
    if(!thermal_fan_set_enable(true)) { return false; }
    __HAL_TIM_SET_COMPARE(&_fans.timer, _fans.pwm_channel, pwm);
    // PWM_Start will fail if we call it twice, so make sure the fan is 
    // off before calling it
    if(old_power == 0.0F) {
        if(HAL_TIM_PWM_Start(&_fans.timer, _fans.pwm_channel) != HAL_OK) {
            // Disable 12v if we couldn't set the PWM
            thermal_fan_set_enable(false);
            return false;
        }
    }

    return true;
}

double thermal_fan_get_power(void) {
    return _fans.power;
}

void thermal_fan_tim4_msp_init(void) {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* Peripheral clock enable */
    __HAL_RCC_TIM4_CLK_ENABLE();

    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**TIM4 GPIO Configuration
    PD12     ------> TIM4_CH1
    PD13     ------> TIM4_CH2
    */
    GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* TIM4 DMA Init */
    /* TIM4_CH1 Init */
    _fans.tach.tach_1_dma.Instance = DMA1_Channel2;
    _fans.tach.tach_1_dma.Init.Request = DMA_REQUEST_TIM4_CH1;
    _fans.tach.tach_1_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    _fans.tach.tach_1_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    _fans.tach.tach_1_dma.Init.MemInc = DMA_MINC_ENABLE;
    _fans.tach.tach_1_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    _fans.tach.tach_1_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    _fans.tach.tach_1_dma.Init.Mode = DMA_NORMAL;
    _fans.tach.tach_1_dma.Init.Priority = DMA_PRIORITY_LOW;
    hal_ret = HAL_DMA_Init(&_fans.tach.tach_1_dma);
    configASSERT(hal_ret == HAL_OK);

    __HAL_LINKDMA(&_fans.tach.timer,hdma[TIM_DMA_ID_CC1],_fans.tach.tach_1_dma);

    /* TIM4_CH2 Init */
    _fans.tach.tach_2_dma.Instance = DMA1_Channel3;
    _fans.tach.tach_2_dma.Init.Request = DMA_REQUEST_TIM4_CH2;
    _fans.tach.tach_2_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    _fans.tach.tach_2_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    _fans.tach.tach_2_dma.Init.MemInc = DMA_MINC_ENABLE;
    _fans.tach.tach_2_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    _fans.tach.tach_2_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    _fans.tach.tach_2_dma.Init.Mode = DMA_NORMAL;
    _fans.tach.tach_2_dma.Init.Priority = DMA_PRIORITY_LOW;
    hal_ret = HAL_DMA_Init(&_fans.tach.tach_2_dma);
    configASSERT(hal_ret == HAL_OK);

    __HAL_LINKDMA(&_fans.tach.timer,hdma[TIM_DMA_ID_CC2],_fans.tach.tach_2_dma);
    
    /* TIM4 interrupt Init */
    HAL_NVIC_SetPriority(TIM4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}


double thermal_fan_get_tach_1_rpm(void) {
    if(_fans.tach.tach_1_period == 0) {
        return 0;
    }
    return ((double)SEC_PER_MIN * (double)TACH_TIMER_PRESCALED_FREQ) 
            / ((double)_fans.tach.tach_1_period * PULSES_PER_ROTATION);
}

double thermal_fan_get_tach_2_rpm(void) {
    if(_fans.tach.tach_2_period == 0) {
        return 0;
    }
    return ((double)SEC_PER_MIN * (double)TACH_TIMER_PRESCALED_FREQ) 
            / ((double)_fans.tach.tach_2_period * PULSES_PER_ROTATION);
}

// Local function implementations

static void thermal_fan_init_tach(struct Tachometer *tach) {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};

    tach->timer.Instance = TIM4;
    tach->timer.Init.Prescaler = TACH_TIMER_PRESCALE;
    tach->timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    tach->timer.Init.Period = TACH_TIMER_RELOAD;
    tach->timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tach->timer.Init.RepetitionCounter = 0;
    tach->timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_IC_Init(&tach->timer);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(&tach->timer, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 0;
    hal_ret = HAL_TIM_IC_ConfigChannel(&tach->timer, &sConfigIC, TIM_CHANNEL_1);
    configASSERT(hal_ret == HAL_OK);
    hal_ret = HAL_TIM_IC_ConfigChannel(&tach->timer, &sConfigIC, TIM_CHANNEL_2);
    configASSERT(hal_ret == HAL_OK);

    // Set timer to One Pulse Mode
    tach->timer.Instance->CR1 |= TIM_OPMODE_SINGLE;

    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    /* DMA1_Channel2_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
    /* DMA1_Channel3_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}

static void thermal_fan_restart_tach_dma() {
    // The interrupt only checks the last entry in the array to decide whether
    // the fan was moving, so it's ok to leave the rest
    _fans.tach.buffer_1[TACH_NUM_READINGS - 1] = 0;
    _fans.tach.buffer_2[TACH_NUM_READINGS - 1] = 0;
    HAL_DMA_Abort_IT(&_fans.tach.tach_1_dma);
    HAL_DMA_Abort_IT(&_fans.tach.tach_2_dma);
    HAL_DMA_Start_IT(&_fans.tach.tach_1_dma, (uint32_t)&_fans.tach.timer.Instance->CCR1,
                     (uint32_t)_fans.tach.buffer_1, TACH_NUM_READINGS);
    HAL_DMA_Start_IT(&_fans.tach.tach_2_dma, (uint32_t)&_fans.tach.timer.Instance->CCR2,
                     (uint32_t)_fans.tach.buffer_2, TACH_NUM_READINGS);

    __HAL_TIM_ENABLE(&_fans.tach.timer);
}

static void thermal_fan_setup_tach_timer() {
    __HAL_TIM_ENABLE_IT(&_fans.tach.timer, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE_DMA(&_fans.tach.timer, TIM_DMA_CC1);
    __HAL_TIM_ENABLE_DMA(&_fans.tach.timer, TIM_DMA_CC2);
    TIM_CCxChannelCmd(_fans.tach.timer.Instance, TIM_CHANNEL_1, TIM_CCx_ENABLE);
    TIM_CCxChannelCmd(_fans.tach.timer.Instance, TIM_CHANNEL_2, TIM_CCx_ENABLE);
}

static bool thermal_fan_set_enable(bool enabled) {
    if(!_fans.initialized) { return false; }

    GPIO_PinState state = (enabled) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(_fans.enable_port, _fans.enable_pin, state);

    return true;
}

// This interrupt does NOT go through the HAL system because it doesn't
// work with the requirements for this timer application.
// For each tachometer, check whether there's a valid pair of Input Capture
// values. If so, the difference between them is the updated period for this
// tachometer. Otherwise the period is 0 to indicate an inactive fan.
void TIM4_IRQHandler(void) {
    if(__HAL_TIM_GET_FLAG(&_fans.tach.timer, TIM_IT_UPDATE)) {
        __HAL_TIM_CLEAR_IT(&_fans.tach.timer, TIM_IT_UPDATE);

        if(_fans.tach.buffer_1[2] > _fans.tach.buffer_1[1]) {
            _fans.tach.tach_1_period = _fans.tach.buffer_1[2] - _fans.tach.buffer_1[1];
        } else {
            _fans.tach.tach_1_period = 0;
        }
        if(_fans.tach.buffer_2[2] > _fans.tach.buffer_2[1]) {
            _fans.tach.tach_2_period = _fans.tach.buffer_2[2] - _fans.tach.buffer_2[1];
        } else {
            _fans.tach.tach_2_period = 0;
        }
        
        thermal_fan_restart_tach_dma();
    }
}

void DMA1_Channel2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&_fans.tach.tach_1_dma);
}

/**
  * @brief This function handles DMA1 channel3 global interrupt.
  */
void DMA1_Channel3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&_fans.tach.tach_2_dma);
}
