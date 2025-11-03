#include "firmware/pump_hardware.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_it.h"
#include "stm32g4xx_hal_tim.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define TIMER_CLOCK_FREQ (170000000)

// Pump PWM timer reload frequency
#define PWM_FREQUENCY    (25000)  // 25Khz
#define PWM_PRESCALER    (0)
#define PWM_PERIOD       ((TIMER_CLOCK_FREQ / (PWM_FREQUENCY * (PWM_PRESCALER + 1))) - 1)
#define MAX_PWM (PWM_PERIOD + 1)

// Tachometer timer reload frequency
#define TACH_FREQUENCY   (1000000)  // 1Mhz
#define TACH_PRESCALER   (0)
#define TACH_PERIOD      ((TIMER_CLOCK_FREQ / (TACH_FREQUENCY * (TACH_PRESCALER + 1))) - 1)

#define TACH_TIMER_PRESCALED_FREQ (TIMER_CLOCK_FREQ / (TACH_PERIOD + 1))
#define SEC_PER_MIN (60)
#define PULSES_PER_ROTATION (2)
#define PULSES_PER_CAPTURE (8)

#define TACH_TIMER (TIM3)
#define TACH_CHANNEL (TIM_CHANNEL_4)
#define TACH_IRQ (TIM3_IRQn)

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim17;

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

static tachometer_hardware_t hardware = {
    .timer = {0},
    .last_ccr = 0,
    .tach_period = 0,
    .pulse_in_this_period = false,
    .initialized = false,
    .initialization_started = false,
};

void pump_pwm_timer_init(void) {
	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

    htim17.Instance = TIM17;
    htim17.Init.Prescaler = PWM_PRESCALER;
    htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim17.Init.Period = PWM_PERIOD;
    htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim17.Init.RepetitionCounter = 0;
    htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim17) != HAL_OK) {
        Error_Handler();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim17, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Init(&htim17) != HAL_OK) {
        Error_Handler();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim17, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
    sConfigOC.Pulse = 0;
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim17, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
        Error_Handler();
    }
    HAL_TIM_MspPostInit(&htim17);
}

static void init_tach_timer(TIM_HandleTypeDef *handle) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};

    handle->Instance = TACH_TIMER;
    handle->State = HAL_TIM_STATE_RESET;
    handle->Init.Prescaler = TACH_PRESCALER;
    handle->Init.CounterMode = TIM_COUNTERMODE_UP;
    handle->Init.Period = TACH_PERIOD;
    handle->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    handle->Init.RepetitionCounter = 0;
    handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if(HAL_TIM_IC_Init(handle) != HAL_OK) {
        Error_Handler();
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(handle, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }

    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV8;
    sConfigIC.ICFilter = 0;

    if (HAL_TIM_IC_ConfigChannel(handle, &sConfigIC, TACH_CHANNEL) != HAL_OK) {
        Error_Handler();
    }
}

void pump_pwm_gpio_init() {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* Peripheral clock enable */
    __HAL_RCC_TIM17_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /**TIM17 GPIO Configuration
    PA7     ------> TIM17_CH1
    */
    GPIO_InitStruct.Pin = PUMP_PWM_GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM17;
    HAL_GPIO_Init(PUMP_PWM_GPIO_Port, &GPIO_InitStruct);
}

void pump_tach_gpio_init() {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* Peripheral clock enable */
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = PUMP_TACH_GPIO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(PUMP_TACH_GPIO_Port, &GPIO_InitStruct);

    /* Timer interrupt Init */
    HAL_NVIC_SetPriority(TACH_IRQ, 0, 0);
    HAL_NVIC_EnableIRQ(TACH_IRQ);
}

void pump_tach_timer_init() {
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

void hw_start_pump_motor(bool start) {
    start ? HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1) : HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);
}

void hw_set_pump_duty_cycle(int16_t duty) {
    if (duty > 100) duty = 100;
    if (duty < 0) duty = 0;
    uint16_t ccr = (uint16_t)(((float)duty / 100.0f) * (PWM_PERIOD + 1));
    TIM17->CCR1 = ccr;
}

uint16_t hw_get_pump_duty_cycle(void) {
    float duty = ((float)TIM17->CCR1 / (PWM_PERIOD + 1)) * 100.0f;
    if (duty > 100) duty = 100;
    if (duty < 0) duty = 0;
    return (uint16_t)(duty);
}

double hw_get_pump_rpm(void) {
    // If we directly use the atomic variable after the if(),
    // there's a chance we can divide by zero!
    long period = hardware.tach_period;

    if(period == 0) {
      return 0;
    }

    return (double)period;
    //
    // return ((double)SEC_PER_MIN * (double)(PULSES_PER_CAPTURE) * 
    //         (double)TACH_TIMER_PRESCALED_FREQ)
    //         / ((double)period * PULSES_PER_ROTATION);
}

void pump_hardware_init(void) {
    pump_tach_gpio_init();
    pump_pwm_gpio_init();
    pump_tach_timer_init();
    pump_pwm_timer_init();

    // Turn off pump
    hw_set_pump_duty_cycle(75);
    hw_start_pump_motor(true);
}

// This interrupt does NOT go through the HAL system because that overhead is
// not required for this application.
void TIM1_TRG_COM_TIM3_IRQHandler(void) {
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
