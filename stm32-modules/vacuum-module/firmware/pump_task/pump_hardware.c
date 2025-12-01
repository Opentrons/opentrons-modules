#include "firmware/pump_hardware.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_tim.h"

#define TIMER_CLOCK_HZ     (170000000)  // 170Mhz
#define PWM_FREQUENCY_HZ   (25000)  // 25Khz
#define PWM_PRESCALER      (0)
#define PWM_PERIOD         ((TIMER_CLOCK_HZ / (PWM_FREQUENCY_HZ * (PWM_PRESCALER + 1))) - 1)
#define MAX_PWM            (PWM_PERIOD + 1)

#define TACH_PRESCALER     (5)    // Timer ticks at ~28.33 MHzA
#define TACH_PERIOD        (0xFFFFUL)  // 16-bit timer max
#define TACH_TIMER_FREQ     (TIMER_CLOCK_HZ / (TACH_PRESCALER + 1))

#define PWM_TIMER          (TIM17)
#define PWM_CHANNEL        (TIM_CHANNEL_1)
#define TACH_TIMER         (TIM3)
#define TACH_CHANNEL       (TIM_CHANNEL_4)
#define TACH_IRQ           (TIM3_IRQn)

#define MIN_RPM 1
#define MAX_RPM 5000
#define RPM_AVG_WINDOW 6
#define TACH_TRANSITIONS_PER_REV 30.0f

#define PUMP_STOP_TIMEOUT_MS   500   // no pulses for n >= ms -> stopped
#define PUMP_STOP_RPM_THRESH   40.0f // filtered RPM < n ->  likely stopped
#define PUMP_STOP_DEBOUNCE     3     // require 3 consecutive detections

#define PID_FILTER_ALPHA  0.01f  // Reacts fast (good for control) [WORKING]
#define STARTUP_BLIND_TIME_MS  100 // Ignore tach for n  ms after start

// Define the minimum ticks allowed between pulses.
// 170MHz / 6 (prescaler 5) = 28.33MHz.
// 28.33MHz * 100us = ~2833 ticks.
#define MIN_VALID_DELTA_TICKS  2800


typedef struct {
    // tachometer
    // uint32_t last_ccr;
    atomic_long period;
    // bool valid_capture;
    // _Atomic uint32_t tach_overflow_count;
    // rolling average rpm
    _Atomic float rpm;
    float buffer[RPM_AVG_WINDOW];
    uint8_t index;
    uint8_t count;
    float sum;
    float rpm_filtered;

    // pump stopped detection
    uint32_t last_pulse_time_ms;
    uint8_t stopped_counter;
    bool pump_stopped;

    // NEW --------------------------
    // tachometer raw capture
    uint32_t last_ccr;
    volatile uint32_t tach_overflow_count;
    bool valid_capture;

    // rolling average of TICKS (Time), not RPM
    uint32_t tick_buffer[RPM_AVG_WINDOW];
    uint32_t running_tick_sum; // Sum of the buffer
    uint8_t  buf_index;
    uint8_t  valid_samples;    // To handle startup cleanly

    uint32_t pump_start_time;

} tachometer_hardware_t;

static tachometer_hardware_t hardware = {0};


void update_pump_stopped_state(void);

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
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_BOTHEDGE;
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

void reset_rpm_filtered() {
    // Safely clear the hardware struct
    portENTER_CRITICAL();
    memset(&hardware, 0, sizeof(tachometer_hardware_t));
    hardware.valid_capture = false;
    portEXIT_CRITICAL();
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
    hardware.pump_start_time = HAL_GetTick();
    return HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1) == HAL_OK;
}

bool hw_stop_pump_motor() {
    hardware.pump_start_time = 0;
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
        reset_rpm_filtered();
        __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_UPDATE);
        success = HAL_TIM_IC_Start_IT(&htim3, TACH_CHANNEL) == HAL_OK;
    } else {
        __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_UPDATE);
        success = HAL_TIM_IC_Stop_IT(&htim3, TACH_CHANNEL) == HAL_OK;
        reset_rpm_filtered();
    }

    return success;
}

float hw_get_pump_rpm(void) {
    // check if the motor has stopped
    update_pump_stopped_state();
    if (hardware.pump_stopped) return 0.0f;
    // check blint start time to allow the motor time to overcome friction
    if ((HAL_GetTick() - hardware.pump_start_time) < STARTUP_BLIND_TIME_MS) {
        return 0.0f;
    }

    uint32_t total_ticks;
    portENTER_CRITICAL();
    total_ticks = hardware.running_tick_sum;
    portEXIT_CRITICAL();

    if (total_ticks == 0) return 0.0f;

    float numerator = (float)RPM_AVG_WINDOW * (float)TACH_TIMER_FREQ * 60.0f;
    float denominator = (float)total_ticks * TACH_TRANSITIONS_PER_REV;
    float current_raw_rpm = numerator / denominator;

    // Filter to Smooth out the piston compression ripple
    if (hardware.rpm_filtered == 0.0f) {
        hardware.rpm_filtered = current_raw_rpm;
    } else {
        hardware.rpm_filtered = (PID_FILTER_ALPHA * current_raw_rpm) +
                                    ((1.0f - PID_FILTER_ALPHA) * hardware.rpm_filtered);
    }

    return hardware.rpm_filtered;
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

void update_rpm_filtered(float new_rpm) {
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

    hardware.rpm_filtered = hardware.sum / hardware.count;
}


float tach_ticks_to_rpm(uint32_t tick_period) {
    if (tick_period == 0) {
        return 0.0f;
    }
    float period_s = (float)tick_period / (TIMER_CLOCK_HZ / (TACH_PRESCALER + 1));
    float fgout_freq = 1.0f / period_s;
    float rev_per_s = fgout_freq / TACH_TRANSITIONS_PER_REV;
    return rev_per_s * 60.0f;
}

void update_pump_stopped_state(void) {
    uint32_t now = HAL_GetTick();

    // Check timeout
    uint32_t elapsed = now - hardware.last_pulse_time_ms;

    if (elapsed >= PUMP_STOP_TIMEOUT_MS) {
        if (hardware.stopped_counter < PUMP_STOP_DEBOUNCE) {
            hardware.stopped_counter++;
        }
    } else {
        hardware.stopped_counter = 0;
    }

    hardware.pump_stopped = (hardware.stopped_counter >= PUMP_STOP_DEBOUNCE);
}

void tach_period_overflow_callback(void) {
    hardware.tach_overflow_count += 1;
    // TODO: maybe have cb here to inform task?
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TACH_TIMER) {
        hardware.last_pulse_time_ms = HAL_GetTick();

        uint32_t current_capture = HAL_TIM_ReadCapturedValue(htim, TACH_CHANNEL);
        uint32_t overflows = hardware.tach_overflow_count;

        if (!hardware.valid_capture) {
            hardware.last_ccr = current_capture;
            hardware.valid_capture = true;
            hardware.tach_overflow_count = 0;
            return;
        }

        // 2. Calculate Ticks since last pulse (Delta)
        // (overflows * 65536) + (current - last)
        // Note: Using standard math. If current < last, it handles borrowing,
        // but we need to account for the full overflow counts.
        uint32_t delta_ticks = 0;
        if (current_capture >= hardware.last_ccr) {
             delta_ticks = (overflows * (TACH_PERIOD + 1)) + (current_capture - hardware.last_ccr);
        } else {
             // Timer wrapped within the calculation window
             // Note: overflows should be at least 1 here if wrap occurred
             delta_ticks = ((overflows - 1) * (TACH_PERIOD + 1)) + ((TACH_PERIOD + 1 - hardware.last_ccr) + current_capture);
        }

        // If the pulse came too fast (e.g., < 100us), it's electrical noise.
        // Ignore this capture, but DONT update last_ccr.
        // We effectively "wait" for the real edge.
        if (delta_ticks < MIN_VALID_DELTA_TICKS) {
            // Do not reset overflow count here, because we are continuing to count time
            // until the next valid edge.
            return;
        }

        // Reset tracking for next loop
        hardware.last_ccr = current_capture;
        hardware.tach_overflow_count = 0;

        // 3. Ring Buffer: Update Running Sum (Subtract Oldest, Add Newest)
        hardware.running_tick_sum -= hardware.tick_buffer[hardware.buf_index];
        hardware.tick_buffer[hardware.buf_index] = delta_ticks;
        hardware.running_tick_sum += delta_ticks;

        // 4. Advance Index
        hardware.buf_index++;
        if (hardware.buf_index >= RPM_AVG_WINDOW) {
            hardware.buf_index = 0;
        }

        // 5. Startup Logic
        if (hardware.valid_samples < RPM_AVG_WINDOW) {
            hardware.valid_samples++;
        }
    }
}
