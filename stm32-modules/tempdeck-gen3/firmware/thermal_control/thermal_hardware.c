#include "firmware/thermal_hardware.h"

// HAL includes
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"

// RTOS includes
#include "FreeRTOS.h"
#include "task.h"

// ***************************************************************************
// Local constants

#define PULSE_WIDTH_FREQ (500000)
#define TIMER_CLOCK_FREQ (170000000)
// These two together give a 25kHz pulse width, and the ARR value
// of 99 gives us a nice scale of 0-100 for the pulse width.
// A finer scale is possible by reducing the prescale value and
// adjusting the reload to match.
#define TIM1_PRESCALER (0)
#define TIM1_RELOAD ((TIMER_CLOCK_FREQ / (PULSE_WIDTH_FREQ * (TIM1_PRESCALER + 1))) - 1)
// PWM should be scaled from 0 to MAX_PWM, inclusive
#define MAX_PWM (TIM1_RELOAD + 1)

#define COOLING_CHANNEL (TIM_CHANNEL_1)
#define COOLING_PORT (GPIOC)
#define COOLING_PIN (GPIO_PIN_0)

#define HEATING_CHANNEL (TIM_CHANNEL_2)
#define HEATING_PORT (GPIOC)
#define HEATING_PIN (GPIO_PIN_1)

#define PELTIER_ENABLE_PORT (GPIOA)
#define PELTIER_ENABLE_PIN (GPIO_PIN_0)

#define ENABLE_12V_PORT (GPIOA)
#define ENABLE_12V_PIN (GPIO_PIN_3)

// Peltier drive circuitry cannot support lower PWM than 0.1
#define MIN_PELTIER_POWER (0.1)
// Peltier drive circuitry cannot support higher PWM than 0.9
#define MAX_PELTIER_POWER (0.9)

// ***************************************************************************
// Local typedefs

struct thermal_hardware {
    TIM_HandleTypeDef timer;
    bool initialized;
    bool enabled;
    double cool_side_power;
    double hot_side_power;
};


// ***************************************************************************
// Local variables

static struct thermal_hardware hardware = {
    .timer = {},
    .initialized = false,
    .enabled = false,
    .cool_side_power = 0.0F,
    .hot_side_power = 0.0F,
};

// ***************************************************************************
// Static function declaration

/**
 * @brief Initializes the Peltier's timer, TIM1
 */
static void init_peltier_timer();

/**
 * @brief Initializes the GPIO lines for the 12v Enable and Peltier Enable,
 * and enables the 12v rail on the PCB.
 */
static void init_gpio();

// ***************************************************************************
// Public function implementation

void thermal_hardware_init() {
    if(!hardware.initialized) {
        init_gpio();
        init_peltier_timer();

        hardware.initialized = true;
    }
}

void thermal_hardware_enable_peltiers() {
    if(!hardware.initialized) {
        return;
    }
    hardware.enabled = true;
    HAL_GPIO_WritePin(PELTIER_ENABLE_PORT, PELTIER_ENABLE_PIN, GPIO_PIN_SET);
}

void thermal_hardware_disable_peltiers() {
    if(!hardware.initialized) {
        return;
    }
    hardware.enabled = false;
    HAL_GPIO_WritePin(PELTIER_ENABLE_PORT, PELTIER_ENABLE_PIN, GPIO_PIN_RESET);
    
    __HAL_TIM_SET_COMPARE(&hardware.timer, HEATING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.timer, COOLING_CHANNEL, 0);

    hardware.hot_side_power = 0.0F;
    hardware.cool_side_power = 0.0F;
}

bool thermal_hardware_set_peltier_heat(double power) {
    if((!hardware.initialized) || (!hardware.enabled)) {
        return false;
    }

    if(power < MIN_PELTIER_POWER && power > 0.0F) {
        power = MIN_PELTIER_POWER;
    }
    if(power < 0.0F) {
        power = 0.0F;
    }
    if(power > MAX_PELTIER_POWER) {
        power = MAX_PELTIER_POWER;
    }

    uint32_t pwm = power * (double)MAX_PWM;
    __HAL_TIM_SET_COMPARE(&hardware.timer, COOLING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.timer, HEATING_CHANNEL, pwm);
    
    hardware.hot_side_power = power;
    hardware.cool_side_power = 0.0F;

    return true;
}

bool thermal_hardware_set_peltier_cool(double power) {
    if((!hardware.initialized) || (!hardware.enabled)) {
        return false;
    }

    if(power < MIN_PELTIER_POWER && power > 0.0F) {
        power = MIN_PELTIER_POWER;
    }
    if(power < 0.0F) {
        power = 0.0F;
    }
    if(power > MAX_PELTIER_POWER) {
        power = MAX_PELTIER_POWER;
    }
    
    uint32_t pwm = power * (double)MAX_PWM;
    __HAL_TIM_SET_COMPARE(&hardware.timer, HEATING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.timer, COOLING_CHANNEL, pwm);
    
    hardware.hot_side_power = 0.0F;
    hardware.cool_side_power = power;

    return true;
}


// ***************************************************************************
// Static function implementation

static void init_peltier_timer() {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    hardware.timer.Instance = TIM1;
    hardware.timer.Init.Prescaler = TIM1_PRESCALER;
    hardware.timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    hardware.timer.Init.Period = TIM1_RELOAD;
    hardware.timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    hardware.timer.Init.RepetitionCounter = 0;
    hardware.timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_PWM_Init(&hardware.timer);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(&hardware.timer, &sMasterConfig);
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

    hal_ret = HAL_TIM_PWM_ConfigChannel(&hardware.timer,
                                        &sConfigOC,
                                        HEATING_CHANNEL);
    configASSERT(hal_ret == HAL_OK);

    hal_ret = HAL_TIM_PWM_ConfigChannel(&hardware.timer,
                                        &sConfigOC,
                                        COOLING_CHANNEL);
    configASSERT(hal_ret == HAL_OK);

    // Breaktime settings are all default
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
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(&hardware.timer, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);
    
    // Set up the PWM GPIO pins 

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PC0     ------> TIM1_CH1
    PC1     ------> TIM1_CH2
    */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM1;
    GPIO_InitStruct.Pin = HEATING_PIN;
    HAL_GPIO_Init(HEATING_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = COOLING_PIN;
    HAL_GPIO_Init(COOLING_PORT, &GPIO_InitStruct);

    // Activate both PWM channels with a compare val of 0
    __HAL_TIM_SET_COMPARE(&hardware.timer, HEATING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.timer, COOLING_CHANNEL, 0);
    HAL_TIM_PWM_Start(&hardware.timer, HEATING_CHANNEL);
    HAL_TIM_PWM_Start(&hardware.timer, COOLING_CHANNEL);
}

static void init_gpio() {
    GPIO_InitTypeDef init = {0};

    __GPIOA_CLK_ENABLE();

    init.Pin = ENABLE_12V_PIN;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_LOW;

    HAL_GPIO_Init(ENABLE_12V_PORT, &init);

    init.Pin = PELTIER_ENABLE_PIN;
    HAL_GPIO_Init(PELTIER_ENABLE_PORT, &init);

    HAL_GPIO_WritePin(ENABLE_12V_PORT, ENABLE_12V_PIN, GPIO_PIN_SET);
}
