#include <stdlib.h>

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_adc.h"
#include "stm32f3xx_hal_adc_ex.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_cortex.h"

#include "heater_hardware.h"

static void init_error(void);
static void adc_setup(ADC_HandleTypeDef* adc);
static void gpio_setup(void);
static ntc_selection next_channel(ntc_selection from_which);

typedef struct {
    ntc_selection reading_which;
    conversion_results results;
} hw_internal;

hw_internal _internals;

heater_hardware *HEATER_HW_HANDLE = NULL;

#define NTC_PAD_A_PIN (1<<1)
#define NTC_PAD_A_PORT GPIOB
#define NTC_PAD_B_PIN (1<<9)
#define NTC_PAD_B_PORT GPIOE
#define NTC_BOARD_PIN (1<<13)
#define NTC_BOARD_PORT GPIOE

void gpio_setup(void) {
    GPIO_InitTypeDef gpio_init = {
    .Pin = (NTC_PAD_B_PIN | NTC_BOARD_PIN),
    .Mode = GPIO_MODE_ANALOG,
    .Pull = 0,
    .Alternate = 0,
    };
    HAL_GPIO_Init(NTC_PAD_B_PORT, &gpio_init);
    gpio_init.Pin = NTC_PAD_A_PIN;
    HAL_GPIO_Init(NTC_PAD_A_PORT, &gpio_init);
}

void adc_setup(ADC_HandleTypeDef* adc) {
    adc->Instance = ADC3;
    adc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
    adc->Init.Resolution = ADC_RESOLUTION_12B;
    adc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    adc->Init.ScanConvMode = ADC_SCAN_DISABLE;
    adc->Init.ContinuousConvMode = DISABLE;
    adc->Init.NbrOfConversion = 3; // pad a is channel 1, b is 2, onboard is 3
    adc->Init.DiscontinuousConvMode = ENABLE;
    adc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    adc->Init.DMAContinuousRequests = DISABLE;
    if (HAL_OK != HAL_ADC_Init(adc)) {
        init_error();
    }
    ADC_ChannelConfTypeDef channel_conf = {
    .Channel = NTC_PAD_A,
    .Rank = ADC_REGULAR_RANK_1,
    .SamplingTime = ADC_SAMPLETIME_19CYCLES_5,
    };
    if (HAL_OK != HAL_ADC_ConfigChannel(adc, &channel_conf)) {
        init_error();
    }
    channel_conf.Channel = NTC_PAD_B;
    channel_conf.Rank = ADC_REGULAR_RANK_2;
    if (HAL_OK != HAL_ADC_ConfigChannel(adc, &channel_conf)) {
        init_error();
    }
    channel_conf.Channel = NTC_ONBOARD;
    channel_conf.Rank = ADC_REGULAR_RANK_3;
    if (HAL_OK != HAL_ADC_ConfigChannel(adc, &channel_conf)) {
        init_error();
    }
    HAL_ADCEx_Calibration_Start(adc, ADC_SINGLE_ENDED);
}

void heater_hardware_setup(heater_hardware* hardware) {
    HEATER_HW_HANDLE = hardware;
    hardware->hardware_internal = (void*)&_internals;
    _internals.reading_which = NTC_PAD_A;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_ADC34_CLK_ENABLE();
    gpio_setup();
    adc_setup(&hardware->ntc_adc);
    HAL_NVIC_SetPriority(ADC3_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(ADC3_IRQn);
    HAL_ADC_Start(&hardware->ntc_adc);
}

void heater_hardware_teardown(heater_hardware* hardware) {
    HAL_NVIC_DisableIRQ(ADC3_IRQn);
    HAL_ADC_Stop(&hardware->ntc_adc);
    __HAL_RCC_ADC34_CLK_DISABLE();
}

void heater_hardware_begin_conversions(heater_hardware* hardware) {
    HAL_ADC_Start_IT(&hardware->ntc_adc);
}


// Actual IRQ handler to call into the HAL IRQ handler
void ADC3_IRQHandler(void) {
    if (HEATER_HW_HANDLE) {
        HAL_ADC_IRQHandler(&HEATER_HW_HANDLE->ntc_adc);
    }
}

ntc_selection next_channel(ntc_selection from_which) {
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
    ntc_selection which = internal->reading_which;
    internal->reading_which = next_channel(which);

    if (hadc != &HEATER_HW_HANDLE->ntc_adc) {
        return;
    }
    switch(which) {
        case NTC_PAD_A: {
            internal->results.pad_a_val = HAL_ADC_GetValue(&HEATER_HW_HANDLE->ntc_adc);
            break;}
        case NTC_PAD_B: {
            internal->results.pad_b_val = HAL_ADC_GetValue(&HEATER_HW_HANDLE->ntc_adc);
            break;}
        case NTC_ONBOARD: {
            internal->results.onboard_val = HAL_ADC_GetValue(&HEATER_HW_HANDLE->ntc_adc);
            if (HEATER_HW_HANDLE->conversions_complete) {
                HEATER_HW_HANDLE->conversions_complete(&internal->results);
            }
            break;}
    }


}


static void init_error(void) {
    while (1);
}
