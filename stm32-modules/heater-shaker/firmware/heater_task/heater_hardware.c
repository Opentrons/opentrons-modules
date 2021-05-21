#include <stdlib.h>

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_adc.h"
#include "stm32f3xx_hal_adc_ex.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_cortex.h"
#include "stm32f3xx_hal_tim.h"

#include "heater_hardware.h"

static void init_error(void);
static void adc_setup(ADC_HandleTypeDef* adc);
static void gpio_setup(void);
static ntc_selection next_channel(ntc_selection from_which);
static void tim_setup(TIM_HandleTypeDef* tim);

typedef struct {
    ntc_selection reading_which;
    conversion_results results;
    ADC_HandleTypeDef ntc_adc;
    TIM_HandleTypeDef pad_tim;
} hw_internal;

hw_internal _internals;

heater_hardware *HEATER_HW_HANDLE = NULL;

#define NTC_PAD_A_PIN (1<<1)
#define NTC_PAD_A_PORT GPIOB
#define NTC_PAD_B_PIN (1<<9)
#define NTC_PAD_B_PORT GPIOE
#define NTC_BOARD_PIN (1<<13)
#define NTC_BOARD_PORT GPIOE
#define HEATER_PGOOD_SENSE_PORT GPIOD
#define HEATER_PGOOD_SENSE_PIN (1<<12)
#define HEATER_PGOOD_LATCH_PORT GPIOD
#define HEATER_PGOOD_LATCH_PIN (1<<13)
#define HEATER_PAD_ENABLE_PORT GPIOD
#define HEATER_PAD_ENABLE_PIN (1<<14)
#define HEATER_PAD_ENABLE_TIM_CHANNEL TIM_CHANNEL_4


static void gpio_setup(void) {
    // NTC sense pis all routed to the ADC
    GPIO_InitTypeDef gpio_init = {
    .Pin = (NTC_PAD_B_PIN | NTC_BOARD_PIN),
    .Mode = GPIO_MODE_ANALOG,
    .Pull = 0,
    .Alternate = 0,
    };
    HAL_GPIO_Init(NTC_PAD_B_PORT, &gpio_init);
    gpio_init.Pin = NTC_PAD_A_PIN;
    HAL_GPIO_Init(NTC_PAD_A_PORT, &gpio_init);

    // Power good sense pin GPIO input nopull
    gpio_init.Pin = HEATER_PGOOD_SENSE_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    HAL_GPIO_Init(HEATER_PGOOD_SENSE_PORT, &gpio_init);

    // Power good latch pin GPIO output pullup to ensure
    // it doesn't affect the latch when not driven
    gpio_init.Pin = HEATER_PGOOD_LATCH_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(HEATER_PGOOD_SENSE_PORT, &gpio_init);
    HAL_GPIO_WritePin(HEATER_PGOOD_LATCH_PORT,
                      HEATER_PGOOD_LATCH_PIN,
                      GPIO_PIN_SET);
    gpio_init.Pin = HEATER_PAD_ENABLE_PIN;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(HEATER_PAD_ENABLE_PORT, &gpio_init);
}

static void adc_setup(ADC_HandleTypeDef* adc) {
    adc->Instance = ADC3;
    adc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
    adc->Init.Resolution = ADC_RESOLUTION_12B;
    adc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->Init.ScanConvMode = ADC_SCAN_DISABLE;
    adc->Init.ContinuousConvMode = DISABLE;
    adc->Init.NbrOfConversion = 1;
    adc->Init.DiscontinuousConvMode = ENABLE;
    adc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc->Init.DMAContinuousRequests = DISABLE;
    if (HAL_OK != HAL_ADC_Init(adc)) {
        init_error();
    }

    HAL_ADCEx_Calibration_Start(adc, ADC_SINGLE_ENDED);
}

static void tim_setup(TIM_HandleTypeDef* tim) {
    tim->Instance = TIM4;
    tim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->Init.Prescaler = HEATER_PAD_TIM_PRESCALER;
    tim->Init.RepetitionCounter = 0;
    tim->Init.AutoReloadPreload = HEATER_PAD_PWM_GRANULARITY;
    TIM_OC_InitTypeDef channel_config = {
        .OCMode = TIM_OCMODE_PWM1,
        .Pulse = HEATER_PAD_PWM_GRANULARITY,
        .OCPolarity = TIM_OCPOLARITY_HIGH,
        .OCIdleState = TIM_OCIDLESTATE_RESET
    };
    if (HAL_OK != HAL_TIM_PWM_Init(tim)) {
        init_error();
    }
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(tim, &channel_config, HEATER_PAD_ENABLE_TIM_CHANNEL)) {
        init_error();
    }
}

void heater_hardware_setup(heater_hardware* hardware) {
    HEATER_HW_HANDLE = hardware;
    hardware->hardware_internal = (void*)&_internals;
    _internals.reading_which = NTC_PAD_A;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_ADC34_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    gpio_setup();
    adc_setup(&_internals.ntc_adc);
    tim_setup(&_internals.pad_tim);
    HAL_NVIC_SetPriority(ADC3_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(ADC3_IRQn);
    HAL_ADC_Start(&_internals.ntc_adc);
}

void heater_hardware_teardown(heater_hardware* hardware) {
    hw_internal* internal = (hw_internal*)hardware->hardware_internal;
    if (!internal) {
        return;
    }
    HAL_NVIC_DisableIRQ(ADC3_IRQn);
    HAL_ADC_Stop(&internal->ntc_adc);
    __HAL_RCC_ADC34_CLK_DISABLE();
}

void heater_hardware_begin_conversions(heater_hardware* hardware) {
    ADC_ChannelConfTypeDef channel_conf = {
        .Channel = NTC_PAD_A,
        .Rank = ADC_REGULAR_RANK_1,
        .SamplingTime = ADC_SAMPLETIME_19CYCLES_5,
    };
    hw_internal* internal = (hw_internal*)hardware->hardware_internal;
    if (!internal) {
        init_error();
    }
    internal->reading_which = NTC_PAD_A;
    if (HAL_OK != HAL_ADC_ConfigChannel(&internal->ntc_adc, &channel_conf)) {
        init_error();
    }

    HAL_ADC_Start_IT(&internal->ntc_adc);
}

bool heater_hardware_sense_power_good() {
    return (HAL_GPIO_ReadPin(HEATER_PGOOD_SENSE_PORT, HEATER_PGOOD_SENSE_PIN)
            == GPIO_PIN_SET);
}

void heater_hardware_drive_pg_latch_low() {
    HAL_GPIO_WritePin(HEATER_PGOOD_LATCH_PORT,
                      HEATER_PGOOD_LATCH_PIN,
                      GPIO_PIN_RESET);
}

void heater_hardware_release_pg_latch() {
    HAL_GPIO_WritePin(HEATER_PGOOD_LATCH_PORT,
                      HEATER_PGOOD_LATCH_PIN,
                      GPIO_PIN_SET);
}


// Actual IRQ handler to call into the HAL IRQ handler
void ADC3_IRQHandler(void) {
    if (HEATER_HW_HANDLE && HEATER_HW_HANDLE->hardware_internal) {
        hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
        HAL_ADC_IRQHandler(&internal->ntc_adc);
    }
}

static ntc_selection next_channel(ntc_selection from_which) {
    switch (from_which) {
        case NTC_PAD_A:
            return NTC_PAD_B;
        case NTC_PAD_B:
            return NTC_ONBOARD;
        case NTC_ONBOARD:
            return NTC_PAD_A;
        default:
            return NTC_PAD_A;
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (!HEATER_HW_HANDLE || !HEATER_HW_HANDLE->hardware_internal) {
        return;
    }
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
    if (hadc != &internal->ntc_adc) {
        return;
    }

    ntc_selection which = internal->reading_which;
    internal->reading_which = next_channel(which);

    ADC_ChannelConfTypeDef channel_conf = {
      .Channel = internal->reading_which,
      .Rank = ADC_REGULAR_RANK_1,
      .SamplingTime = ADC_SAMPLETIME_19CYCLES_5,
    };
    switch(which) {
        case NTC_PAD_A: {
            internal->results.pad_a_val = HAL_ADC_GetValue(&internal->ntc_adc);
            if (HAL_OK != HAL_ADC_ConfigChannel(&internal->ntc_adc, &channel_conf)) {
                init_error();
            }
            HAL_ADC_Start_IT(&internal->ntc_adc);
            break;}
        case NTC_PAD_B: {
            internal->results.pad_b_val = HAL_ADC_GetValue(&internal->ntc_adc);
            if (HAL_OK != HAL_ADC_ConfigChannel(&internal->ntc_adc, &channel_conf)) {
                init_error();
            }
            HAL_ADC_Start_IT(&internal->ntc_adc);
            break;}
        case NTC_ONBOARD: {
            internal->results.onboard_val = HAL_ADC_GetValue(&internal->ntc_adc);
            if (HEATER_HW_HANDLE->conversions_complete) {
                HEATER_HW_HANDLE->conversions_complete(&internal->results);
            }
            break;}
    }
}

static void init_error(void) {
    while (1);
}
