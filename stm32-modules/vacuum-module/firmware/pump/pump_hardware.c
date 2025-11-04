#include "firmware/pump_hardware.h"
#include "main.h"

#include <math.h>
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
#define PULSES_PER_ROTATION (4)
#define PULSES_PER_CAPTURE (8)

#define TACH_TIMER (TIM3)
#define TACH_CHANNEL (TIM_CHANNEL_4)
#define TACH_IRQ (TIM3_IRQn)

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim17;


// The rotor speed should be determined by taking the moving average of the
// time between the last 6 FG transitions. This will represent one electrical
// cycle of the motor which will help smooth out any errors as a result of
// misalignment of hall effect sensors relative to the rotor.

#define RPM_AVG_WINDOW 6

typedef struct {
    float buffer[RPM_AVG_WINDOW];
    uint8_t index;
    uint8_t count;
    float sum;
    float filtered_rpm;
} TachFilter_t;

TachFilter_t tach_filter = {0};

typedef struct {
    TIM_HandleTypeDef timer;
    // Holds the previous Capture Compare value, for computing the period
    uint32_t last_ccr;
    // Most recent period calculation. Set in interrupt and read by 
    // thread contexts. Stored in units of raw timer counts.
    float tach_period;
    // Has there been an overflow in this timer period?
    bool pulse_in_this_period;
    int edges;

    atomic_bool initialized;
    atomic_bool initialization_started;
} tachometer_hardware_t;

static tachometer_hardware_t hardware = {
    .timer = {0},
    .last_ccr = 0,
    .tach_period = 0.0f,
    .edges = 0,
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
    (void)handle;

    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};

    htim3.Instance = TACH_TIMER;
    htim3.State = HAL_TIM_STATE_RESET;
    htim3.Init.Prescaler = TACH_PRESCALER;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = TACH_PERIOD;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
        Error_Handler();

    /* Initialize Input Capture mode */
    if (HAL_TIM_IC_Init(&htim3) != HAL_OK) Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
        Error_Handler();
    }
    /* Initialize TIM3 input capture channel */
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim3, &sConfigIC, TACH_CHANNEL) != HAL_OK) {
        Error_Handler();
    }

    // /* TIM3 interrupt Init */
    HAL_NVIC_SetPriority(TACH_IRQ, 0, 0);
    HAL_NVIC_EnableIRQ(TACH_IRQ);
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
}

void pump_tach_timer_init() {
    if(atomic_exchange(&hardware.initialization_started, true) == false) {
        init_tach_timer(&hardware.timer);
        hardware.initialized = true;
    } else {
        // Spin until the hardware is initialized
        while(!hardware.initialized) {
            taskYIELD();
        }
    }
}

static uint32_t clamp(uint32_t val, uint32_t min, uint32_t max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

void hw_start_pump_motor(bool start) {
    start ? HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1) : HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);
}

void hw_set_pump_duty_cycle(int16_t duty) {
    duty = clamp(duty, 0, 100);
    uint16_t ccr = (uint16_t)(((float)duty / 100.0f) * (PWM_PERIOD + 1));
    TIM17->CCR1 = ccr;
}

uint16_t hw_get_pump_duty_cycle(void) {
    float duty = ((float)TIM17->CCR1 / (PWM_PERIOD + 1)) * 100.0f;
    return (uint16_t)(clamp(duty, 0, 100));
}

bool hw_enable_pump_tach(bool enable) {
    if (enable) {
        __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);
        return HAL_TIM_IC_Start_IT(&htim3, TACH_CHANNEL) == HAL_OK;
    } else {
        __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_UPDATE);
        return HAL_TIM_IC_Stop_IT(&htim3, TACH_CHANNEL) == HAL_OK;
    }
}

float hw_get_pump_rpm(void) {
    // If we directly use the atomic variable after the if(),
    // there's a chance we can divide by zero!
    float period = hardware.tach_period;

    if(period == 0) {
      return 0;
    }

    float edges = (float)hardware.edges;
    hardware.edges = 0;
    return edges;

    // return (float)period;
    // return ((float)SEC_PER_MIN * (float)(PULSES_PER_CAPTURE) *
    //         (float)TACH_TIMER_PRESCALED_FREQ)
    //         / ((float)period * PULSES_PER_ROTATION);
}

void pump_hardware_init(void) {
    pump_tach_gpio_init();
    pump_pwm_gpio_init();
    pump_tach_timer_init();
    pump_pwm_timer_init();

    // Turn off pump
    hw_set_pump_duty_cycle(50);
    hw_start_pump_motor(true);
    hw_enable_pump_tach(true);
}


void update_tach_filter(float new_rpm) {
    // Subtract the oldest sample from the sum
    tach_filter.sum -= tach_filter.buffer[tach_filter.index];

    // Store new sample
    tach_filter.buffer[tach_filter.index] = new_rpm;
    tach_filter.sum += new_rpm;

    // Update index (circular)
    tach_filter.index++;
    if (tach_filter.index >= RPM_AVG_WINDOW)
        tach_filter.index = 0;

    // Track how many samples weve accumulated
    if (tach_filter.count < RPM_AVG_WINDOW)
        tach_filter.count++;

    // Compute average
    tach_filter.filtered_rpm = tach_filter.sum / tach_filter.count;
}


float old_val = 0.0f;
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TACH_TIMER) {
        hardware.edges += 1;
        if (old_val == 0.0f) {
            old_val = (float)HAL_TIM_ReadCapturedValue(htim, TACH_CHANNEL);
        } else {
            float new_val = (float)HAL_TIM_ReadCapturedValue(htim, TACH_CHANNEL);
            float period = new_val > old_val ? new_val - old_val : ((0xffffffff - old_val) + new_val) + 1;
            hardware.tach_period = 1.0f / (period / 1000);
            if (hardware.edges > 6) {
                hardware.edges = 0;
                update_tach_filter(hardware.tach_period);
            }
            old_val = 0.0f;
            __HAL_TIM_SET_COUNTER(htim, 0);
        }
    }
}

