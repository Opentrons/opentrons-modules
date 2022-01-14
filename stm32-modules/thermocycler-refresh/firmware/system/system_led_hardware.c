/**
 * @file system_led_hardware.c
 * @brief Hardware control code specific to the firmware LED hardware
 * @details
 * Timer 17 is configured for PWM output on its only channel (CH1) and
 * set up wtih a DMA 
 */

// My header
#include "firmware/system_led_hardware.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "task.h"
// HAL includes
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_dma.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"

// ----------------------------------------------------------------------------
// LOCAL DEFINITIONS


#define PULSE_WIDTH_FREQ (800000)
#define TIMER_CLOCK_FREQ (170000000)
// These two together give a 400kHz total period
#define TIM17_PRESCALER (0)
#define TIM17_RELOAD (424)
// PWM should be scaled from 0 to MAX_PWM, inclusive
#define MAX_PWM (TIM17_RELOAD + 1)

struct led_hardware {
    TIM_HandleTypeDef tim; // Timer handle
    DMA_HandleTypeDef dma; // DMA handle
    const uint32_t tim_channel;
    TaskHandle_t task_to_notify;
    bool initialized;
};

// ----------------------------------------------------------------------------
// LOCAL VARIABLES

static struct led_hardware _leds = {
    .tim = {0},
    .dma = {0},
    .tim_channel = TIM_CHANNEL_1,
    .task_to_notify = NULL,
    .initialized = false,
};

// ----------------------------------------------------------------------------
// PUBLIC FUNCTION IMPLEMENTATION

void system_led_iniitalize(void) {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* DMA controller clock enable */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel1_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    /* DMAMUX_OVR_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMAMUX_OVR_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMAMUX_OVR_IRQn);

    _leds.tim.Instance = TIM17;
    _leds.tim.Init.Prescaler = TIM17_PRESCALER;
    _leds.tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    _leds.tim.Init.Period = TIM17_RELOAD;
    _leds.tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    _leds.tim.Init.RepetitionCounter = 0;
    _leds.tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_Base_Init(&_leds.tim);
    configASSERT(hal_ret == HAL_OK);

    hal_ret = HAL_TIM_PWM_Init(&_leds.tim);
    configASSERT(hal_ret == HAL_OK);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    hal_ret = HAL_TIM_PWM_ConfigChannel(&_leds.tim, &sConfigOC, _leds.tim_channel);
    configASSERT(hal_ret == HAL_OK);

    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(&_leds.tim, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);

    // This is generated as the "post-init" function from STM32Cube
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**TIM17 GPIO Configuration
    PB9     ------> TIM17_CH1
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM17;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    _leds.initialized = true;
}

void system_led_msp_init(void) {
    /* Peripheral clock enable */
    __HAL_RCC_TIM17_CLK_ENABLE();

    /* TIM17 DMA Init */
    /* TIM17_CH1 Init */
    _leds.dma.Instance = DMA1_Channel1;
    _leds.dma.Init.Request = DMA_REQUEST_TIM17_CH1;
    _leds.dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    _leds.dma.Init.PeriphInc = DMA_PINC_DISABLE;
    _leds.dma.Init.MemInc = DMA_MINC_ENABLE;
    _leds.dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    _leds.dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    _leds.dma.Init.Mode = DMA_NORMAL;
    _leds.dma.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&_leds.dma) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(&_leds.tim,hdma[TIM_DMA_ID_CC1],_leds.dma);
}

void system_led_msp_deinit(void) {
    /* Peripheral clock disable */
    __HAL_RCC_TIM17_CLK_DISABLE();

    /* TIM17 DMA DeInit */
    HAL_DMA_DeInit(_leds.tim.hdma[TIM_DMA_ID_CC1]);
}

bool system_led_start_send(uint16_t *buffer, size_t len) {
    if((buffer == NULL) || (!_leds.initialized)) {
        return false;
    }
    if(_leds.task_to_notify != NULL) {
        // Another transmission is in progress
        return false;
    }

    _leds.task_to_notify = xTaskGetCurrentTaskHandle();
    xTaskNotifyStateClear(_leds.task_to_notify);

    HAL_StatusTypeDef hal_ret = 
        HAL_TIM_PWM_Start_DMA(&_leds.tim, 
                              _leds.tim_channel,
                              (uint32_t*)buffer,
                              len);
    return hal_ret == HAL_OK;
}

void system_led_stop(void) {
    (void)HAL_TIM_PWM_Stop_DMA(&_leds.tim, _leds.tim_channel);
    _leds.task_to_notify = NULL;
}

uint16_t system_led_max_pwm(void) {
    return MAX_PWM;
}

bool system_led_wait_for_interrupt(uint32_t timeout) {
    TickType_t max_block_time = pdMS_TO_TICKS(timeout);
    if(_leds.task_to_notify == NULL) {
        return false;
    }
    uint32_t notification_val = ulTaskNotifyTake(pdFALSE, max_block_time);

    return (notification_val == 1);
}

/**
  * @brief This function handles DMA1 channel1 global interrupt.
  */
void DMA1_Channel1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&_leds.dma);
}

void system_led_pulse_callback(void) {
    /**
     * Increments the value of the active task's Notification Index 0, without
     * clearing the handle for the task. In this way, if the DMA is ready for
     * new data before the task gets to the blocking stage it won't be an
     * issue, the task will just go without blocking.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if( _leds.task_to_notify == NULL ) {
        return;
    }
    vTaskNotifyGiveFromISR( _leds.task_to_notify, &xHigherPriorityTaskWoken );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
