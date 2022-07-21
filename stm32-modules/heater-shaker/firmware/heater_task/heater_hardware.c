#include <stdlib.h>
#include <stddef.h>

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_adc.h"
#include "stm32f3xx_hal_adc_ex.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_cortex.h"
#include "stm32f3xx_hal_tim.h"
#include "stm32f3xx_ll_tim.h"
#include "stm32f3xx_hal_dac.h"
#include "stm32f3xx_hal_comp.h"

#include "heater_hardware.h"
#include "systemwide.h"

static void init_error(void);
static void adc_setup(ADC_HandleTypeDef* adc);
static void gpio_setup(void);
static ntc_selection next_channel(ntc_selection from_which);
static void tim_setup(TIM_HandleTypeDef* tim);
bool peripherals_ready(void);
bool can_heatpad_cs_open_check(void);
bool can_heatpad_cs_short_check(void);
bool heatpad_in_error_state(void);

typedef struct {
    ntc_selection reading_which;
    conversion_results results;
    ADC_HandleTypeDef ntc_adc;
    TIM_HandleTypeDef pad_tim;
    DAC_HandleTypeDef dac1;
    COMP_HandleTypeDef comp4;
    TIM_OC_InitTypeDef pwm_config;
    bool heater_started;
    heatpad_cs_state heatpad_cs_status;
    uint16_t pwm_pulse_duration;
    uint16_t period_count;
    bool update_lock;
} hw_internal;

hw_internal _internals = {
.reading_which = NTC_PAD_A,
.results = {0, 0, 0},
.ntc_adc = {},
.pad_tim = {},
.pwm_config = {
.OCMode = TIM_OCMODE_PWM1,
.Pulse = 0,
.OCPolarity = TIM_OCPOLARITY_HIGH,
.OCNPolarity = TIM_OCNPOLARITY_HIGH,
.OCFastMode = TIM_OCFAST_DISABLE,
.OCIdleState=TIM_OCIDLESTATE_RESET,
.OCNIdleState=TIM_OCNIDLESTATE_RESET,
},
.heater_started = false,
.heatpad_cs_status = IDLE,
.pwm_pulse_duration = 0,
.period_count = 0,
.update_lock = false,
};

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
#define HEATER_PAD_ENABLE_TIM_CHANNEL TIM_CHANNEL_3
#define HEATER_PAD_OPEN_CHECK_TIM_CHANNEL TIM_CHANNEL_2
#define HEATER_PAD_SHORT_CHECK_TIM_CHANNEL TIM_CHANNEL_4
#define HEATER_PAD_OUTPUT_ACTIVE_CHANNEL HAL_TIM_ACTIVE_CHANNEL_3
#define HEATER_PAD_OPEN_CHECK_ACTIVE_CHANNEL HAL_TIM_ACTIVE_CHANNEL_2
#define HEATER_PAD_SHORT_CHECK_ACTIVE_CHANNEL HAL_TIM_ACTIVE_CHANNEL_4
#define HEATER_PAD_LL_SETCOMPARE LL_TIM_OC_SetCompareCH3
#define HEATPAD_CS_DAC_CHANNEL DAC_CHANNEL_1
#define HEATPAD_CS_PIN GPIO_PIN_0
#define HEATPAD_CS_PORT GPIOB

static const uint32_t OFFSETS_PAGE_ADDRESS = 0x0807F000; //second last page in flash memory. Last page reserved for serial number storage

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
    gpio_init.Pin = HEATPAD_CS_PIN;
    HAL_GPIO_Init(HEATPAD_CS_PORT, &gpio_init);

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
    gpio_init.Pull = GPIO_NOPULL;
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
    tim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    tim->Init.Period = HEATER_PAD_PWM_GRANULARITY;
    TIM_OC_InitTypeDef channel_config = {
        .OCMode = TIM_OCMODE_TIMING,
        .Pulse = HEATER_PAD_PWM_GRANULARITY,
        .OCPolarity = TIM_OCPOLARITY_HIGH,
        .OCIdleState = TIM_OCIDLESTATE_RESET
    };
    TIM_OC_InitTypeDef channel_config_2 = {
        .OCMode = TIM_OCMODE_TIMING,
        .Pulse = HEATER_PAD_SHORT_CHECK_PULSE,
        .OCPolarity = TIM_OCPOLARITY_HIGH,
        .OCIdleState = TIM_OCIDLESTATE_RESET
    };
    TIM_OC_InitTypeDef channel_config_3 = {
        .OCMode = TIM_OCMODE_TIMING,
        .Pulse = HEATER_PAD_OPEN_CHECK_PULSE,
        .OCPolarity = TIM_OCPOLARITY_HIGH,
        .OCIdleState = TIM_OCIDLESTATE_RESET
    };
    if (HAL_OK != HAL_TIM_PWM_Init(tim)) {
        init_error();
    }
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(tim, &channel_config, HEATER_PAD_ENABLE_TIM_CHANNEL)) {
        init_error();
    }
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(tim, &channel_config_2, HEATER_PAD_SHORT_CHECK_TIM_CHANNEL)) {
        init_error();
    }
    if (HAL_OK != HAL_TIM_PWM_ConfigChannel(tim, &channel_config_3, HEATER_PAD_OPEN_CHECK_TIM_CHANNEL)) {
        init_error();
    }
}

static void dac_setup(DAC_HandleTypeDef* dac) {
  __HAL_RCC_DAC1_CLK_ENABLE();
  dac->Instance = DAC1;
  if (HAL_OK != HAL_DAC_Init(dac)) {
    init_error();
  }

  DAC_ChannelConfTypeDef chan_config = {
     .DAC_Trigger = DAC_TRIGGER_NONE,
     .DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE,
  };
  HAL_DAC_ConfigChannel(dac, &chan_config, HEATPAD_CS_DAC_CHANNEL);
  HAL_DAC_Start(dac, HEATPAD_CS_DAC_CHANNEL);
  HAL_DAC_SetValue(dac, HEATPAD_CS_DAC_CHANNEL, DAC_ALIGN_12B_R, HEATER_PAD_OVERCURRENT_DAC_THRESHOLD);
}

static void comp_setup(COMP_HandleTypeDef* comp) {
  comp->Instance = COMP4;
  comp->Init.InvertingInput = COMP_INVERTINGINPUT_DAC1_CH1;
  comp->Init.NonInvertingInput = COMP_NONINVERTINGINPUT_IO1; //PB0
  comp->Init.Output = COMP_OUTPUT_TIM1BKIN2;
  if (HAL_OK != HAL_COMP_Init(comp)) {
    init_error();
  }
  HAL_COMP_Start(comp);
}

void heater_hardware_setup(heater_hardware* hardware) {
    HEATER_HW_HANDLE = hardware;
    hardware->hardware_internal = (void*)&_internals;
    _internals.reading_which = NTC_PAD_A;
    _internals.heatpad_cs_status = IDLE;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_ADC34_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    gpio_setup();
    adc_setup(&_internals.ntc_adc);
    tim_setup(&_internals.pad_tim);
    dac_setup(&_internals.dac1);
    comp_setup(&_internals.comp4);
    HAL_NVIC_SetPriority(TIM4_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
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
        .SamplingTime = ADC_SAMPLETIME_601CYCLES_5,
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

void heater_hardware_power_disable(heater_hardware* hardware) {
    hw_internal* internal = (hw_internal*)hardware->hardware_internal;
    if (!internal) {
        init_error();
    }
    HAL_TIM_PWM_Stop_IT(&internal->pad_tim, HEATER_PAD_ENABLE_TIM_CHANNEL);
    HAL_TIM_PWM_Stop_IT(&internal->pad_tim, HEATER_PAD_SHORT_CHECK_TIM_CHANNEL);
    HAL_TIM_PWM_Stop_IT(&internal->pad_tim, HEATER_PAD_OPEN_CHECK_TIM_CHANNEL);
    internal->heater_started = false;
    if (!heatpad_in_error_state()) {
        internal->heatpad_cs_status = IDLE;
    }
}

HEATPAD_CIRCUIT_ERROR heater_hardware_power_set(heater_hardware* hardware, uint16_t setting) {
    hw_internal* internal = (hw_internal*)hardware->hardware_internal;
    if (!internal) {
        init_error();
    }
    if (heatpad_in_error_state()) {
        if (internal->heatpad_cs_status == ERROR_SHORT_CIRCUIT) {
            return SHORT;
        } else if (internal->heatpad_cs_status == ERROR_OPEN_CIRCUIT) {
            return OPEN;
        } else if (internal->heatpad_cs_status == ERROR_OVERCURRENT) {
            return OVERCURRENT;
        }
    }
    if (internal->heatpad_cs_status == IDLE) {
        internal->heatpad_cs_status = RUNNING;
    }
    if (!internal->update_lock) {
        internal->pwm_pulse_duration = setting;
        internal->pwm_config.Pulse = setting;
        if (!internal->heater_started) {
            HAL_TIM_PWM_Stop_IT(&internal->pad_tim, HEATER_PAD_ENABLE_TIM_CHANNEL);
            HAL_TIM_PWM_ConfigChannel(
                &internal->pad_tim, &internal->pwm_config, HEATER_PAD_ENABLE_TIM_CHANNEL);
            HAL_TIM_PWM_Start_IT(&internal->pad_tim, HEATER_PAD_ENABLE_TIM_CHANNEL);
            internal->pwm_config.Pulse = HEATER_PAD_SHORT_CHECK_PULSE;
            HAL_TIM_PWM_Stop_IT(&internal->pad_tim, HEATER_PAD_SHORT_CHECK_TIM_CHANNEL);
            HAL_TIM_PWM_ConfigChannel(
                &internal->pad_tim, &internal->pwm_config, HEATER_PAD_SHORT_CHECK_TIM_CHANNEL);
            HAL_TIM_PWM_Start_IT(&internal->pad_tim, HEATER_PAD_SHORT_CHECK_TIM_CHANNEL);
            internal->pwm_config.Pulse = HEATER_PAD_OPEN_CHECK_PULSE;
            HAL_TIM_PWM_Stop_IT(&internal->pad_tim, HEATER_PAD_OPEN_CHECK_TIM_CHANNEL);
            HAL_TIM_PWM_ConfigChannel(
                &internal->pad_tim, &internal->pwm_config, HEATER_PAD_OPEN_CHECK_TIM_CHANNEL);
            HAL_TIM_PWM_Start_IT(&internal->pad_tim, HEATER_PAD_OPEN_CHECK_TIM_CHANNEL);
            internal->heater_started = true;
        } else {
            HEATER_PAD_LL_SETCOMPARE(internal->pad_tim.Instance, setting);
        }
    }
    return NONE;
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
      .SamplingTime = ADC_SAMPLETIME_601CYCLES_5,
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

bool heater_hardware_set_offsets(struct writable_offsets* to_write) {
    FLASH_EraseInitTypeDef pageToErase = {.TypeErase = FLASH_TYPEERASE_PAGES, .PageAddress = OFFSETS_PAGE_ADDRESS, .NbPages = 1};
    uint32_t pageErrorPtr = 0; //pointer to variable  that contains the configuration information on faulty page in case of error
    uint32_t ProgramAddress1 = OFFSETS_PAGE_ADDRESS + offsetof(struct writable_offsets, const_b); //offsetof returns bytes
    uint32_t ProgramAddress2 = OFFSETS_PAGE_ADDRESS + offsetof(struct writable_offsets, const_c);
    uint32_t ProgramAddress3 = OFFSETS_PAGE_ADDRESS + offsetof(struct writable_offsets, const_flag);

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status == HAL_OK) {
        status = HAL_FLASHEx_Erase(&pageToErase, &pageErrorPtr);
        if (status == HAL_OK) {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ProgramAddress1, to_write->const_b);
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ProgramAddress2, to_write->const_c);
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ProgramAddress3, to_write->const_flag);
            if (status == HAL_OK) {
                status = HAL_FLASH_Lock();
            }
        }
    }
    return (status == HAL_OK);
}

uint64_t heater_hardware_get_offset(size_t addr_offset) {
    uint32_t AddressToRead = OFFSETS_PAGE_ADDRESS + addr_offset; //addr_offset in bytes
    return *(uint64_t*)AddressToRead;
}

// This callback attempts to check for heatpad open and short circuit conditions via 
// the heatpad current sensing pin once per second. If the output pwm pulse is greater
// than 20% of the timer period, the current sensing pin will be checked to be greater 
// than 0.2V at 10% of the timer period to confirm there is no open circuit. If the 
// output pwm pulse is less than 80% of the timer period, the current sensing pin will 
// be checked to be less than 0.2V at 90% of the timer period to confirm there is no 
// shorted circuit. After these checks are completed, the comparator threshold is 
// returned to 3.2V to detect an overcurrent condition. TIM4 channels 2 and 4 are used 
// to trigger this callback at 10% and 90% of the timer period. This state machine 
// incorporates preparation states to ensure the DAC and comparator have enough time to 
// adjust their settings before the comparator level is checked.
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
    if ((htim->Instance == TIM4) && (htim->Channel != HEATER_PAD_OUTPUT_ACTIVE_CHANNEL)) {
        if ((internal->heatpad_cs_status != IDLE) && (!heatpad_in_error_state())) {
            internal->period_count++;
            if ((internal->heatpad_cs_status == RUNNING) && (internal->period_count > HEATER_PAD_CIRCUIT_CHECK_PERIOD) && (htim->Channel == HEATER_PAD_SHORT_CHECK_ACTIVE_CHANNEL)) {
                internal->update_lock = true;
                HAL_DAC_SetValue(&internal->dac1, HEATPAD_CS_DAC_CHANNEL, DAC_ALIGN_12B_R, HEATER_PAD_CIRCUIT_DAC_THRESHOLD);
                HAL_COMP_Stop(&internal->comp4);
                internal->comp4.Init.Output = COMP_OUTPUT_NONE;
                if (HAL_OK != HAL_COMP_Init(&internal->comp4)) {
                    init_error();
                }
                HAL_COMP_Start(&internal->comp4);
                if (can_heatpad_cs_open_check()) {
                    internal->heatpad_cs_status = OPEN_CHECK_STARTED;
                } else if (can_heatpad_cs_short_check()) {
                    internal->heatpad_cs_status = SHORT_CHECK_STARTED;
                }
            } else if ((htim->Channel == HEATER_PAD_OPEN_CHECK_ACTIVE_CHANNEL) && (internal->heatpad_cs_status == OPEN_CHECK_STARTED) && (peripherals_ready())) {
                if (!HAL_COMP_GetOutputLevel(&internal->comp4)) {
                    heater_hardware_power_disable(HEATER_HW_HANDLE);
                    internal->heatpad_cs_status = ERROR_OPEN_CIRCUIT;
                } else {
                    if (can_heatpad_cs_short_check()) {
                        internal->heatpad_cs_status = SHORT_CHECK_STARTED;
                    } else {
                        internal->heatpad_cs_status = PREP_RUNNING;
                    }
                }
            } else if ((htim->Channel == HEATER_PAD_SHORT_CHECK_ACTIVE_CHANNEL) && (internal->heatpad_cs_status == SHORT_CHECK_STARTED) && (peripherals_ready())) {
                if (HAL_COMP_GetOutputLevel(&internal->comp4)) {
                    heater_hardware_power_disable(HEATER_HW_HANDLE);
                    internal->heatpad_cs_status = ERROR_SHORT_CIRCUIT;
                } else {
                    internal->heatpad_cs_status = PREP_RUNNING;
                }
            }
            if (internal->heatpad_cs_status == PREP_RUNNING) {
                internal->update_lock = false;
                HAL_COMP_Stop(&internal->comp4);
                HAL_DAC_SetValue(&internal->dac1, HEATPAD_CS_DAC_CHANNEL, DAC_ALIGN_12B_R, HEATER_PAD_OVERCURRENT_DAC_THRESHOLD);
                internal->comp4.Init.Output = COMP_OUTPUT_TIM1BKIN2;
                if (HAL_OK != HAL_COMP_Init(&internal->comp4)) {
                    init_error();
                }
                HAL_COMP_Start(&internal->comp4);
                internal->period_count = 0;
                internal->heatpad_cs_status = RUNNING;
            }
        }
    }
}

bool peripherals_ready(void) {
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
    return ((HAL_DAC_GetState(&internal->dac1) == HAL_DAC_STATE_READY) && (HAL_COMP_GetState(&internal->comp4) == HAL_COMP_STATE_BUSY));
}

bool can_heatpad_cs_open_check(void)
{
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
    return (internal->pwm_pulse_duration > HEATER_PAD_OPEN_CHECK_THRESHOLD);
}

bool can_heatpad_cs_short_check(void)
{
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
    return (internal->pwm_pulse_duration < HEATER_PAD_SHORT_CHECK_THRESHOLD);
}

bool heatpad_in_error_state(void)
{
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
    if ((internal->heatpad_cs_status == ERROR_OPEN_CIRCUIT) || (internal->heatpad_cs_status == ERROR_SHORT_CIRCUIT) || (internal->heatpad_cs_status == ERROR_OVERCURRENT)) {
        return true;
    } else {
        return false;
    }
}

void HAL_TIMEx_Break2Callback(TIM_HandleTypeDef *htim) {
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
    if (htim->Instance == TIM4) {
        internal->heatpad_cs_status = ERROR_OVERCURRENT;
    }
}

void TIM4_IRQHandler(void)
{
    hw_internal* internal = (hw_internal*)HEATER_HW_HANDLE->hardware_internal;
 	HAL_TIM_IRQHandler(&internal->pad_tim);
}

static void init_error(void) {
    while (1);
}
