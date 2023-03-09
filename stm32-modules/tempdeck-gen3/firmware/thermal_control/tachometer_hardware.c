/**
 * @file tachometer_hardware.c
 * @brief This file implements the low-level peripheral hardware to support 
 * the fan tachometer on the Tempdeck.
 * 
 * The tachometer is connected to an Input Compare channel on a timer. The
 * timer is configured to capture its counter value when the tach input
 * switches from low to high, and the software in this file caches this
 * register (CCR1). When a new pulse is received, the value of the most
 * recent CCR1 value is subtracted by the last cached value to give an overall
 * period for the timer. When the RPM is requested, the period is converted
 * to an RPM value based off of the timer configuration.
 * 
 * The timer is set to overflow at 4Hz. If the timer overflows without ever
 * seeing a pulse, then the RPM of the tachometer is set to a hardcoded 0.
 * 
 * Note that the *actual* capture rate is 8x slower than the tachometer pulse
 * input. This alleviates some of the CPU burden for calculating the RPM in
 * real time. There is no impact on the ability to read slow RPM values 
 * because the minimum RPM of the fan is significantly above  the limit of
 * 4Hz * 8 pulses * 2.
 * 
 */


#include "firmware/tachometer_hardware.h"

// Library includes
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

// HAL includes
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_it.h"
#include "stm32g4xx_hal_tim.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

/** Private definitions */

// Timer handle for tachometer
#define TACH_TIMER (TIM17)
// Input channel for tachometer
#define TACH_CHANNEL (TIM_CHANNEL_1)
// Port for tachometer
#define TACH_GPIO_PORT (GPIOA)
// Pin for tachometer
#define TACH_GPIO_PIN  (GPIO_PIN_7)
// Interrupt vector for tach
#define TACH_IRQ (TIM1_TRG_COM_TIM17_IRQn)

// Tachometer timer reload frequency
#define TIMER_CLOCK_FREQ (170000000)
#define TACH_TIMER_FREQ (4)
#define TACH_TIMER_PRESCALE (1699)
#define TACH_TIMER_PRESCALED_FREQ (TIMER_CLOCK_FREQ / (TACH_TIMER_PRESCALE + 1))
#define SEC_PER_MIN (60)
#define PULSES_PER_ROTATION (2)
#define PULSES_PER_CAPTURE (8)

#define TACH_TIMER_RELOAD ((TIMER_CLOCK_FREQ / (TACH_TIMER_FREQ * (TACH_TIMER_PRESCALE + 1))) -1)

/** Private typedef */

typedef struct {
    TIM_HandleTypeDef timer;
    // Holds the previous Capture Compare value, for computing the period
    uint32_t last_ccr;
    // Most recent period calculation. Set in interrupt and read by 
    // thread contexts. Stored in units of raw timer counts.
    atomic_long tach_period;
    // Has there been an overflow in this timer period?
    bool pulse_in_this_period;

    atomic_bool initialized;
    atomic_bool initialization_started;
} tachometer_hardware_t;
/** Static variables */

static tachometer_hardware_t hardware = {
    .timer = {0},
    .last_ccr = 0,
    .tach_period = 0,
    .pulse_in_this_period = false,
    .initialized = false,
    .initialization_started = false,
};

/** Static function declaration */
static void init_tach_timer(TIM_HandleTypeDef *handle);

/** Public functions */

void tachometer_hardware_init() {
    if(atomic_exchange(&hardware.initialization_started, true) == false) {
        init_tach_timer(&hardware.timer);
        HAL_TIM_IC_Start_IT(&hardware.timer, TACH_CHANNEL);
        __HAL_TIM_ENABLE_IT(&hardware.timer, TIM_IT_UPDATE);
        hardware.initialized = true;
    } else {
        // Spin until the hardware is initialized
        while(!hardware.initialized) {
            taskYIELD();
        }
    }
}

double tachometer_hardware_get_rpm() {
    // If we directly use the atomic variable after the if(),
    // there's a chance we can divide by zero!
    long period = hardware.tach_period;
    
    if(period == 0) {
        return 0;
    }

    return ((double)SEC_PER_MIN * (double)(PULSES_PER_CAPTURE) * 
            (double)TACH_TIMER_PRESCALED_FREQ)
            / ((double)period * PULSES_PER_ROTATION);
}

/** Static function implementation */

static void init_tach_timer(TIM_HandleTypeDef *handle) {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};

    configASSERT(handle != NULL);

    handle->Instance = TACH_TIMER;
    handle->State = HAL_TIM_STATE_RESET;
    handle->Init.Prescaler = TACH_TIMER_PRESCALE;
    handle->Init.CounterMode = TIM_COUNTERMODE_UP;
    handle->Init.Period = TACH_TIMER_RELOAD;
    handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    handle->Init.RepetitionCounter = 0;
    handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_IC_Init(handle);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(handle, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV8;
    sConfigIC.ICFilter = 0;

    hal_ret = HAL_TIM_IC_ConfigChannel(handle, &sConfigIC, TACH_CHANNEL);
    configASSERT(hal_ret == HAL_OK);
}

/** Overwritten HAL functions */

// This interrupt does NOT go through the HAL system because that overhead is
// not required for this application.
void TIM1_TRG_COM_TIM17_IRQHandler(void) {
    if(__HAL_TIM_GET_FLAG(&hardware.timer, TIM_IT_CC1)) {
        // New pulse input
        __HAL_TIM_CLEAR_IT(&hardware.timer, TIM_IT_CC1);

        uint32_t ccr = __HAL_TIM_GET_COMPARE(&hardware.timer, TACH_CHANNEL);

        if(hardware.pulse_in_this_period) {
            hardware.tach_period = ccr - hardware.last_ccr;
        }
        
        hardware.last_ccr = ccr;
        hardware.pulse_in_this_period = true;
    }
    if(__HAL_TIM_GET_FLAG(&hardware.timer, TIM_IT_UPDATE)) {
        // Timer overflow is handled after pulses in case both are
        // serviced at the same time.
        __HAL_TIM_CLEAR_IT(&hardware.timer, TIM_IT_UPDATE);
        if(hardware.pulse_in_this_period) {
            hardware.pulse_in_this_period = false;
        } else {
            hardware.tach_period = 0;
        }
    }
}

void HAL_TIM_IC_MspInit(TIM_HandleTypeDef* htim_ic) 
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(htim_ic->Instance==TACH_TIMER)
    {
        /* Peripheral clock enable */
        __HAL_RCC_TIM17_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* CC1 input init */
        GPIO_InitStruct.Pin = TACH_GPIO_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM17;
        HAL_GPIO_Init(TACH_GPIO_PORT, &GPIO_InitStruct);

        /* Timer interrupt Init */
        HAL_NVIC_SetPriority(TACH_IRQ, 5, 0);
        HAL_NVIC_EnableIRQ(TACH_IRQ);
    }
}
