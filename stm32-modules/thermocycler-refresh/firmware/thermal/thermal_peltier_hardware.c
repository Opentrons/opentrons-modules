/**
 * @file thermal_peltier_hardware.c
 * @brief HAL-wrapper code specific to the peltiers.
 * @details
 * Provides control interface for the Peltiers on the system.
 * 
 * Each peltier is controlled by three signals:
 * - An enable pin, which is shared between all drivers and can disable
 * power to all peltiers
 * - A PWM channel, connected to the \b cooling side of the differential
 * driver for the peltier
 * - A simple GPIO output, connected to the \b heating side of the differential
 * driver for the peltier. This is the \b direction \b pin
 * 
 * The peltier is controlled by setting the direction pin to either low or high
 * for cooling or heating, respectively, and then setting the pulse width of
 * the PWM channel to a percentage correlating linearly to the power. Note
 * that, when \b heating a peltier, the PWM is actually \b inversely correlated
 * with the power output. This is because the control is differential, so
 * if the direction pin is high then a 100% PWM will be the same as effectively
 * turning off the peltier.
 * 
 * @note 
 * Separated from thermal_hardware.c in order to reduce filesize.
 */

#include "firmware/thermal_peltier_hardware.h"

#include "FreeRTOS.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"
#include "task.h"

/** Local constants */

#define PULSE_WIDTH_FREQ (25000)
#define TIMER_CLOCK_FREQ (170000000)
// These two together give a 25kHz pulse width, and the ARR value
// of 99 gives us a nice scale of 0-100 for the pulse width.
// A finer scale is possible by reducing the prescale value and
// adjusting the reload to match.
#define TIM1_PRESCALER (67)
//#define TIM1_RELOAD    (99)
#define TIM1_RELOAD ((TIMER_CLOCK_FREQ / (PULSE_WIDTH_FREQ * (TIM1_PRESCALER + 1))) - 1)
// PWM should be scaled from 0 to MAX_PWM, inclusive
#define MAX_PWM (TIM1_RELOAD + 1)

// Port and Pin for peltier enable
#define PELTIER_ENABLE_PORT (GPIOE)
#define PELTIER_ENABLE_PIN  (GPIO_PIN_7)

/** Local typedefs */
typedef struct {
    double power;
    PeltierDirection direction;
    // ID maps to channel directly
    const uint16_t channel;
    // GPIO pins need explicit port/pin
    GPIO_TypeDef *direction_port;
    const uint16_t direction_pin;
} Peltier_t;

struct Peltiers {
    bool initialized;
    bool enabled;
    Peltier_t peltiers[PELTIER_NUMBER];
    TIM_HandleTypeDef timer;
};

/** Local variables */
static struct Peltiers _peltiers = {
    .initialized = false,
    .enabled = false,
    .peltiers = {
        // Right
        {.power = 0.0f,
         .direction = PELTIER_HEATING,
         .channel = TIM_CHANNEL_1,
         .direction_port = GPIOA,
         .direction_pin = GPIO_PIN_7},
        // Center
        {.power = 0.0f,
         .direction = PELTIER_HEATING,
         .channel = TIM_CHANNEL_2,
         .direction_port = GPIOB,
         .direction_pin = GPIO_PIN_0},
        // Left
        {.power = 0.0f,
         .direction = PELTIER_HEATING,
         .channel = TIM_CHANNEL_3,
         .direction_port = GPIOB,
         .direction_pin = GPIO_PIN_1}
    },
    .timer = {}
};

/** Static functions.*/

/**
 * @brief Update output pins for a peltier
 * @param[in] peltier The peltier to update
 */
static void _update_outputs(Peltier_t *peltier) {
    uint32_t pwm = peltier->power * (double)MAX_PWM;
    GPIO_PinState dir_val = GPIO_PIN_RESET;

    if(pwm > MAX_PWM) { pwm = MAX_PWM; }
    // If we're heating, have to reverse the polarity of the pwm
    if(peltier->direction == PELTIER_HEATING) {
        dir_val = GPIO_PIN_SET;
        pwm = MAX_PWM - pwm;
    }
    if(pwm > 0) {
        HAL_GPIO_WritePin(peltier->direction_port,
                        peltier->direction_pin,
                        dir_val);
        __HAL_TIM_SET_COMPARE(&_peltiers.timer, peltier->channel, pwm);
        HAL_TIM_PWM_Start(&_peltiers.timer, peltier->channel);
    } else {
        HAL_GPIO_WritePin(peltier->direction_port,
                        peltier->direction_pin,
                        GPIO_PIN_RESET);
        HAL_TIM_PWM_Stop(&_peltiers.timer, peltier->channel);
    }
    
}

// This is the way STM32CubeMX generates the function. Note that, unlike
// other "MSP" functions, it isn't an override of a weak function somewhere
// in the HAL... instead it's just a brand new function for some reason,
// thus it is stored here rather than with the other MSP init/deinit fxns.
void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(htim->Instance==TIM1)
  {
  /* USER CODE BEGIN TIM1_MspPostInit 0 */

  /* USER CODE END TIM1_MspPostInit 0 */

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PC0     ------> TIM1_CH1
    PC1     ------> TIM1_CH2
    PC2     ------> TIM1_CH3
    */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN TIM1_MspPostInit 1 */

  /* USER CODE END TIM1_MspPostInit 1 */
  }

}

/**
 * @brief Initialize a single peltier on the board.
 * @details Configures:
 * - PWM channel
 * - Direction pin
 * @param[in] peltier The peltier to initialize
 * @param[in] config  The PWM configuration structure for this peltier
 */
void _initialize_peltier(Peltier_t *peltier, TIM_OC_InitTypeDef *config) {
    HAL_StatusTypeDef hal_ret = HAL_TIM_PWM_ConfigChannel(&_peltiers.timer,
                                                          config,
                                                          peltier->channel);
    configASSERT(hal_ret == HAL_OK);
    GPIO_InitTypeDef gpio_init = {
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_HIGH,
        .Pin = peltier->direction_pin
    };
    HAL_GPIO_Init(peltier->direction_port, &gpio_init);
}

/** Public functions.*/

void thermal_peltier_initialize(void) {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    // Set up PWM
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    // Hit all the GPIO clocks (possibly redundant with other
    // init routines, that's fine)
    __GPIOA_CLK_ENABLE();
    __GPIOB_CLK_ENABLE();
    __GPIOE_CLK_ENABLE();

    _peltiers.timer.Instance = TIM1;
    _peltiers.timer.Init.Prescaler = TIM1_PRESCALER;
    _peltiers.timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    _peltiers.timer.Init.Period = TIM1_RELOAD;
    _peltiers.timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    _peltiers.timer.Init.RepetitionCounter = 0;
    _peltiers.timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_PWM_Init(&_peltiers.timer);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(&_peltiers.timer, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    // PWM1 means the output is enabled if the current timer count is LESS THAN
    // the pulse value. Therefore, a pulse of 0 keeps the PWM off all the time,
    // and a pulse of the auto-reload-register + 1 will keep it on 100% of the
    // time.
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    _initialize_peltier(&_peltiers.peltiers[PELTIER_LEFT], &sConfigOC);
    _initialize_peltier(&_peltiers.peltiers[PELTIER_RIGHT], &sConfigOC);
    _initialize_peltier(&_peltiers.peltiers[PELTIER_CENTER], &sConfigOC);

    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
    sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
    sBreakDeadTimeConfig.Break2Filter = 0;
    sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(&_peltiers.timer, &sBreakDeadTimeConfig);

    configASSERT(hal_ret == HAL_OK);
    
    HAL_TIM_MspPostInit(&_peltiers.timer);

    // Initialize enable pin
    GPIO_InitTypeDef gpio_init = {
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_HIGH,
        .Pin = PELTIER_ENABLE_PIN
    };
    HAL_GPIO_Init(PELTIER_ENABLE_PORT, &gpio_init);

    _peltiers.initialized = true;
}

void thermal_peltier_set_enable(const bool enable) {
    if(!_peltiers.initialized) {
        return;
    }
    _peltiers.enabled = enable;
    GPIO_PinState enable_val = (enable) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(PELTIER_ENABLE_PORT,
                      PELTIER_ENABLE_PIN,
                      enable_val);
    if(!enable) {
        __HAL_TIM_DISABLE(&_peltiers.timer);
        // Set all peltiers to 0 power and update their outputs
        for(size_t i = 0; i < PELTIER_NUMBER; ++i) {
            Peltier_t *peltier = &_peltiers.peltiers[i];
            peltier->power = 0.0f;
            peltier->direction = PELTIER_HEATING;
            _update_outputs(peltier);
        }
    } else {
        __HAL_TIM_ENABLE(&_peltiers.timer);
    }
}

bool thermal_peltier_get_enable(void) {
    return _peltiers.enabled;
}

void thermal_peltier_set_power(const PeltierID id, 
                               double power,
                               const PeltierDirection direction) {
    if(!(id < PELTIER_NUMBER)) {
        return;
    }
    if(!_peltiers.enabled) {
        return;
    }
    if((direction != PELTIER_COOLING) && 
       (direction != PELTIER_HEATING)) {
        return;
    }
    // Power is a percentage. Just clamp it instead of complaining.
    if(power < 0.0f) { power = 0.0f; }
    if(power > 1.0f) { power = 1.0f; }
    Peltier_t *peltier = &_peltiers.peltiers[(uint8_t)id];

    peltier->power = power;
    peltier->direction = direction;

    _update_outputs(peltier);
}

bool thermal_peltier_get_power(const PeltierID id, double *power,
                               PeltierDirection *direction) {
    if(!(id < PELTIER_NUMBER)) {
        return false;
    }
    if(power != NULL) {
        *power = _peltiers.peltiers[id].power;
    }
    if(direction != NULL) {
        *direction = _peltiers.peltiers[id].direction;
    }
    return true;
}
