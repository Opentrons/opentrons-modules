#include "firmware/internal_adc_hardware.h"

#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>

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

// Local defines

// Which ADC Peripheral we are initializing
#define ADC_INSTANCE (ADC2)

// Local typedefs

typedef struct {
    ADC_HandleTypeDef adc;
    DMA_HandleTypeDef dma;
    uint16_t readings[INTERNAL_ADC_READING_COUNT];
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
    .dma = {},
    .readings = {0},
    .initialization_started = false,
    .initialized = false,
    .running = false,
};

// Configuration for the Current Measurement ADC Channel
static const adc_channel_init_t imeas_channel_conf = {
    .channel = ADC_CHANNEL_2,
    .pin = GPIO_PIN_1,
    .port = GPIOA
};

static const uint32_t adc_ranks[] = {
    ADC_REGULAR_RANK_1,
    ADC_REGULAR_RANK_2,
    ADC_REGULAR_RANK_3,
    ADC_REGULAR_RANK_4,
    ADC_REGULAR_RANK_5,
    ADC_REGULAR_RANK_6,
    ADC_REGULAR_RANK_7,
    ADC_REGULAR_RANK_8,
    ADC_REGULAR_RANK_9,
    ADC_REGULAR_RANK_10,
};

_Static_assert(sizeof(adc_ranks) / sizeof(adc_ranks[0]) == INTERNAL_ADC_READING_COUNT,
    "ADC ranks length must match number of readings");

// Local function definition

static void init_adc_hardware(ADC_HandleTypeDef *handle);
static void init_dma_hardware(void);

// Public function implementation

void internal_adc_init() {
    if(!atomic_exchange(&adc_hardware.initialization_started, true)) {
        init_dma_hardware();
        init_adc_hardware(&adc_hardware.adc);
        memset(adc_hardware.readings, 0, sizeof(adc_hardware.readings));
        
        adc_hardware.initialized = true;
    } else {
        while(!adc_hardware.initialized) {
            taskYIELD();
        }
    }
}

bool internal_adc_start_readings() {
    HAL_StatusTypeDef ret = HAL_OK;
    if(!adc_hardware.initialized) {
        return false;
    }
    // If the ADC is already running, just exit out.
    // Note that this also sets the running flag,
    if(atomic_exchange(&adc_hardware.running, true)) {
        return true;
    }
    ret = HAL_ADC_Start_DMA(&adc_hardware.adc, 
        // DMA configured for half-word alignment, must cast u16 ptr
        (uint32_t*)adc_hardware.readings, 
        adc_hardware.adc.Init.NbrOfConversion);
    if(ret != HAL_OK) {
        adc_hardware.running = false;
        return false;
    }
    return true;
}

uint32_t internal_adc_get_average() {
    if(adc_hardware.running) {
        return GET_ADC_AVERAGE_ERR;
    }
    // Get the min and max from the available readings, discard them,
    // and return the average of the remaining readings.
    uint32_t min = adc_hardware.readings[0], max = adc_hardware.readings[0];
    uint32_t total = 0;
    for(uint8_t i = 0; i < INTERNAL_ADC_READING_COUNT; ++i) {
        uint32_t reading = adc_hardware.readings[i];
        total += reading;
        min = (reading < min) ? reading : min;
        max = (reading > max) ? reading : max;
    }
    total -= (min + max);
    total /= INTERNAL_ADC_READING_COUNT - 2;
    return total;
}

// Local function implementation

static void init_adc_hardware(ADC_HandleTypeDef *handle) {
    HAL_StatusTypeDef ret = HAL_ERROR;
    ADC_ChannelConfTypeDef channel_config = {0};

    handle->Instance = ADC2;
    handle->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    handle->Init.Resolution = ADC_RESOLUTION_12B;
    handle->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    handle->Init.GainCompensation = 0;
    handle->Init.ScanConvMode = ADC_SCAN_ENABLE;
    handle->Init.EOCSelection = ADC_EOC_SEQ_CONV;
    handle->Init.LowPowerAutoWait = DISABLE;
    handle->Init.ContinuousConvMode = DISABLE;
    handle->Init.NbrOfConversion = INTERNAL_ADC_READING_COUNT;
    handle->Init.DiscontinuousConvMode = DISABLE;
    handle->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    handle->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    handle->Init.DMAContinuousRequests = DISABLE;
    handle->Init.Overrun = ADC_OVR_DATA_PRESERVED;
    handle->Init.OversamplingMode = DISABLE;

    ret = HAL_ADC_Init(handle);
    configASSERT(ret == HAL_OK);

    channel_config.Channel = imeas_channel_conf.channel;
    channel_config.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
    channel_config.SingleDiff = ADC_SINGLE_ENDED;
    channel_config.OffsetNumber = ADC_OFFSET_NONE;
    channel_config.Offset = 0;
    for(uint8_t i = 0; i < INTERNAL_ADC_READING_COUNT; ++i) {
        channel_config.Rank = adc_ranks[i];
        ret = HAL_ADC_ConfigChannel(handle, &channel_config);
        configASSERT(ret == HAL_OK);
    }
}

static void init_dma_hardware(void)
{

    /* DMA controller clock enable */
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel1_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

// Overwritten HAL functions

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *handle)
{
    if(handle->Instance == ADC_INSTANCE) {
        adc_hardware.running = false;
    }
}

/**
* @brief ADC MSP Initialization
* This function configures the hardware resources used in this example
* @param hadc: ADC handle pointer
* @retval None
*/
void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    if(hadc->Instance==ADC_INSTANCE)
    {
        /** Initializes the peripherals clocks
         */
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
        PeriphClkInit.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        {
        Error_Handler();
        }

        /* Peripheral clock enable */
        __HAL_RCC_ADC12_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        
        GPIO_InitStruct.Pin = imeas_channel_conf.pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(imeas_channel_conf.port, &GPIO_InitStruct);

        /* ADC2 DMA Init */
        /* ADC2 Init */
        adc_hardware.dma.Instance = DMA1_Channel1;
        adc_hardware.dma.Init.Request = DMA_REQUEST_ADC2;
        adc_hardware.dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
        adc_hardware.dma.Init.PeriphInc = DMA_PINC_DISABLE;
        adc_hardware.dma.Init.MemInc = DMA_MINC_ENABLE;
        adc_hardware.dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        adc_hardware.dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
        adc_hardware.dma.Init.Mode = DMA_NORMAL;
        adc_hardware.dma.Init.Priority = DMA_PRIORITY_LOW;
        if (HAL_DMA_Init(&adc_hardware.dma) != HAL_OK)
        {
        Error_Handler();
        }

        __HAL_LINKDMA(hadc,DMA_Handle,adc_hardware.dma);

        /* ADC2 interrupt Init */
        HAL_NVIC_SetPriority(ADC1_2_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
    }
}

/**
* @brief ADC MSP De-Initialization
* This function freeze the hardware resources used in this example
* @param hadc: ADC handle pointer
* @retval None
*/
void HAL_ADC_MspDeInit(ADC_HandleTypeDef* hadc)
{
    if(hadc->Instance==ADC_INSTANCE)
    {
        /* Peripheral clock disable */
        __HAL_RCC_ADC12_CLK_DISABLE();

        /**ADC2 GPIO Configuration
        PA1     ------> ADC2_IN2
        */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1);

        HAL_DMA_DeInit(hadc->DMA_Handle);

        /* ADC2 interrupt DeInit */
        HAL_NVIC_DisableIRQ(ADC1_2_IRQn);
    }
}

/**
  * @brief This function handles DMA1 channel1 global interrupt.
  */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&adc_hardware.dma);
}

/**
  * @brief This function handles ADC1 and ADC2 global interrupt.
  */
void ADC1_2_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&adc_hardware.adc);
}
