#include "firmware/internal_adc_hardware.h"

#include <stdbool.h>
#include <stdatomic.h>

// HAL includes
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_it.h"
#include "stm32g4xx_hal_adc.h"
#include "stm32g4xx_hal_adc_ex.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

// Local typedefs

typedef struct {
    ADC_HandleTypeDef adc;
    atomic_bool initialization_started;
    atomic_bool initialized;
    atomic_bool running;
} adc_hardware_t;

typedef struct {
    uint32_t channel;
    uint32_t pin;
    GPIO_TypeDef *port;
} adc_channel_init_t;

// Static variables

static adc_hardware_t adc_hardware = {
    .adc = {},
    .initialization_started = false,
    .initialized = false,
    .running = false,
};

// This array defines the channels to add to the ADC conversion sequence
static const adc_channel_init_t adc_channels[] = {
    {
        .channel = ADC_CHANNEL_2,
        .pin = GPIO_PIN_1,
        .port = GPIOA,
    },
    {
        .channel = ADC_CHANNEL_17,
        .pin = GPIO_PIN_4,
        .port = GPIOA,
    },
    {
        .channel = ADC_CHANNEL_13,
        .pin = GPIO_PIN_5,
        .port = GPIOA,
    },
};

// Local function definition

static void init_adc_hardware(ADC_HandleTypeDef *handle);
static void init_adc_channel(adc_channel_init_t const * channel);

// Public function implementation

void internal_adc_init() {
    if(!atomic_exchange(&adc_hardware.initialization_started, true)) {
        init_adc_hardware();
        adc_hardware.initialized = true;
    } else {
        while(!adc_hardware.initialized) {
            taskYIELD();
        }
    }
}

bool internal_adc_start_reading() {
    if(!adc_hardware.initialized) {
        return false;
    }
    if(adc_hardware.running) {
        return true;
    }
}

// Local function implementation

static void init_adc_hardware(ADC_HandleTypeDef *handle) {
    HAL_StatusTypeDef ret = HAL_ERROR;
    handle->Instance = ADC2;
    handle->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    handle->Init.Resolution = ADC_RESOLUTION_12B;
    handle->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    handle->Init.GainCompensation = 0;
    handle->Init.ScanConvMode = ADC_SCAN_ENABLE;
    handle->Init.EOCSelection = ADC_EOC_SEQ_CONV;
    handle->Init.LowPowerAutoWait = DISABLE;
    handle->Init.ContinuousConvMode = DISABLE;
    handle->Init.NbrOfConversion = 1;
    handle->Init.DiscontinuousConvMode = DISABLE;
    handle->Init.DMAContinuousRequests = DISABLE;
    handle->Init.Overrun = ADC_OVR_DATA_PRESERVED;
    handle->Init.OversamplingMode = DISABLE;

    ret = HAL_ADC_Init(handle);
    configASSERT(ret == HAL_OK);

    
}

static void init_adc_channel(adc_channel_init_t const * channel) {

}
