#include "firmware/pump_hardware.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define TIMER_CLOCK_HZ     (170000000)  // 170Mhz
#define PWM_FREQUENCY_HZ   (25000)  // 25Khz
#define PWM_PRESCALER      (0)
#define PWM_PERIOD         ((TIMER_CLOCK_HZ / (PWM_FREQUENCY_HZ * (PWM_PRESCALER + 1))) - 1)
#define MAX_PWM            (PWM_PERIOD + 1)

#define TACH_PRESCALER     (5)    // chosen to fit 500 Hz - 5 kHz range
#define TACH_PERIOD        (0xFFFFUL)  // 16-bit timer max

#define PWM_TIMER          (TIM17)
#define PWM_CHANNEL        (TIM_CHANNEL_1)
#define TACH_TIMER         (TIM3)
#define TACH_CHANNEL       (TIM_CHANNEL_4)
#define TACH_IRQ           (TIM3_IRQn)

#define MIN_RPM 1
#define MAX_RPM 5000
#define RPM_AVG_WINDOW 10
#define TACH_PULSES_PER_REV 60.0f

#define PUMP_STOP_TIMEOUT_MS   150   // no pulses for n >= ms -> stopped
#define PUMP_STOP_RPM_THRESH   40.0f // filtered RPM < n ->  likely stopped
#define PUMP_STOP_DEBOUNCE     3     // require 3 consecutive detections


typedef struct {
    // tachometer
    uint32_t last_ccr;
    atomic_long period;
    bool valid_capture;
    _Atomic uint32_t tach_overflow_count;
    // rolling average rpm
    _Atomic float rpm;
    float buffer[RPM_AVG_WINDOW];
    uint8_t index;
    uint8_t count;
    float sum;
    float filtered_rpm;
    // pump stopped detection
    uint32_t last_pulse_time_ms;
    uint8_t stopped_counter;
    bool pump_stopped;

} tachometer_hardware_t;

static tachometer_hardware_t hardware = {0};

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim17;

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

static void pump_tach_timer_init(void) {
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
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }
    /* Initialize Input Capture mode */
    if (HAL_TIM_IC_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }
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

void reset_filtered_rpm() {
    memset(&hardware, 0, sizeof(tachometer_hardware_t));
}

static uint32_t clamp(uint32_t val, uint32_t min, uint32_t max) {
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

bool hw_start_pump_motor() {
    return HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1) == HAL_OK;
}

bool hw_stop_pump_motor() {
    return HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1) == HAL_OK;
}

void hw_set_pump_duty_cycle(uint16_t duty) {
    duty = clamp(duty, 0, 100);
    uint16_t ccr = (uint16_t)(((float)duty / 100.0f) * (PWM_PERIOD + 1));
    TIM17->CCR1 = ccr;
}

uint16_t hw_get_pump_duty_cycle(void) {
    float duty = ((float)TIM17->CCR1 / (PWM_PERIOD + 1)) * 100.0f;
    return (uint16_t)(clamp(duty, 0, 100));
}

bool hw_enable_pump_tach(bool enable) {
    bool success = false;
    if (enable) {
        reset_filtered_rpm();
        __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);
        success = HAL_TIM_IC_Start_IT(&htim3, TACH_CHANNEL) == HAL_OK;
    } else {
        __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_UPDATE);
        success = HAL_TIM_IC_Stop_IT(&htim3, TACH_CHANNEL) == HAL_OK;
        reset_filtered_rpm();
    }

    return success;
}

float hw_get_pump_rpm(void) {
    if (hardware.pump_stopped) {
        return 0;
    }
    return hardware.filtered_rpm;
}

void pump_hardware_init(void) {
    pump_tach_gpio_init();
    pump_pwm_gpio_init();
    pump_tach_timer_init();
    pump_pwm_timer_init();

    // Turn off pump
    hw_set_pump_duty_cycle(0);
    hw_start_pump_motor(false);
    hw_enable_pump_tach(false);
}

void update_filtered_rpm(float new_rpm) {
    // Update rpm only if sensible
    if (new_rpm <= MIN_RPM || new_rpm >= MAX_RPM) {
        return;
    }
    // Subtract the oldest sample from the sum and store new sample
    hardware.sum -= hardware.buffer[hardware.index];
    hardware.buffer[hardware.index] = new_rpm;
    hardware.sum += new_rpm;

    // Update index (circular) and count
    hardware.index++;
    if (hardware.index >= RPM_AVG_WINDOW) {
        hardware.index = 0;
    }

    if (hardware.count < RPM_AVG_WINDOW) {
        hardware.count++;
    }

    hardware.filtered_rpm = hardware.sum / hardware.count;
}


float tach_ticks_to_rpm(uint32_t tick_period) {
    if (tick_period == 0) {
        return 0.0f;
    }
    float period_s = (float)tick_period / (TIMER_CLOCK_HZ / (TACH_PRESCALER + 1));
    float fgout_freq = 1.0f / period_s;
    float rev_per_s = fgout_freq / TACH_PULSES_PER_REV;
    return rev_per_s * 60.0f;
}

void update_pump_stopped_state(void) {
    uint32_t now = HAL_GetTick();
    bool detected = false;

    // detected, if last pulse ms is greater than threshold OR rpm is less than threshold
    uint32_t elapsed = now - hardware.last_pulse_time_ms;
    if (elapsed >= PUMP_STOP_TIMEOUT_MS || hardware.filtered_rpm < PUMP_STOP_RPM_THRESH) {
        detected = true;
    }

    // if either are true, then lets debounce
    if (detected) {
        if (hardware.stopped_counter < PUMP_STOP_DEBOUNCE) {
            hardware.stopped_counter++;
        }
    } else {
        hardware.stopped_counter = 0;
    }
    // pump is "stopped" if enough signals have been detected
    hardware.pump_stopped = (hardware.stopped_counter >= PUMP_STOP_DEBOUNCE);
}

void tach_period_overflow_callback(void) {
    hardware.tach_overflow_count += 1;
    update_pump_stopped_state();
    // TODO: maybe have cb here to inform task?
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TACH_TIMER) {
        hardware.last_pulse_time_ms = HAL_GetTick();
        uint32_t current_capture = HAL_TIM_ReadCapturedValue(htim, TACH_CHANNEL);
        if (!hardware.valid_capture) {
            hardware.last_ccr = current_capture;
            hardware.valid_capture = true;
            hardware.tach_overflow_count = 0;
            return;
        }

        // use the tach overflow count times the total number of ticks since
        // the last trigger event to calculate the period.
        uint32_t ticks = hardware.tach_overflow_count * (TACH_PERIOD + 1);
        ticks += current_capture - hardware.last_ccr;
        hardware.last_ccr = current_capture;
        hardware.tach_overflow_count = 0;

        // calculate rpm
        hardware.period = (float)ticks;
        hardware.rpm = tach_ticks_to_rpm(ticks);
        update_filtered_rpm(hardware.rpm);
    }
}
