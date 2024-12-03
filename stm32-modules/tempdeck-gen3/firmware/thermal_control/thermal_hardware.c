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

// Given a desired frequency of 500kHz, we do not need to prescale the timer
#define TIM2_PRESCALER (0)
// Calculates out to 339
#define TIM2_RELOAD ((TIMER_CLOCK_FREQ / (PULSE_WIDTH_FREQ * (TIM2_PRESCALER + 1))) - 1)
// PWM should be scaled from 0 to MAX_PWM, inclusive
#define MAX_PWM (TIM2_RELOAD + 1)

#define HEATING_CHANNEL (TIM_CHANNEL_4)
#define HEATING_PORT (GPIOB)
#define HEATING_PIN (GPIO_PIN_11)

#define COOLING_CHANNEL (TIM_CHANNEL_3)
#define COOLING_PORT (GPIOB)
#define COOLING_PIN (GPIO_PIN_10)

#define PELTIER_ENABLE_PORT (GPIOB)
#define PELTIER_ENABLE_PIN (GPIO_PIN_13)

// Peltier drive circuitry cannot support lower PWM than 0.1
#define MIN_PELTIER_POWER (0.1)
// PWM values over this limit result in overheating of the low-side FET
#define MAX_PELTIER_POWER (0.65)

#define FAN_PWM_Pin (GPIO_PIN_6)
#define FAN_PWM_GPIO_Port (GPIOA)
#define FAN_PULSE_WIDTH_FREQ (1000)

#define TIM16_PRESCALER (67)
// Calculates out to 2499
#define TIM16_RELOAD ((TIMER_CLOCK_FREQ / (FAN_PULSE_WIDTH_FREQ * (TIM16_PRESCALER + 1))) - 1)

// PWM should be scaled from 0 to FAN_MAX_PWM, inclusive
#define FAN_MAX_PWM (TIM16_RELOAD + 1)

#define FAN_CHANNEL (TIM_CHANNEL_1)

// Write Protect config
#define EEPROM_WP_PIN (GPIO_PIN_11)
#define EEPROM_WP_PORT (GPIOC)

// ***************************************************************************
// Local typedefs

struct thermal_hardware {
    TIM_HandleTypeDef peltier_timer;
    TIM_HandleTypeDef fan_timer;
    bool initialized;
    bool enabled;
    double cool_side_power;
    double hot_side_power;
};


// ***************************************************************************
// Local variables

static struct thermal_hardware hardware = {
    .peltier_timer = {},
    .fan_timer = {},
    .initialized = false,
    .enabled = false,
    .cool_side_power = 0.0F,
    .hot_side_power = 0.0F,
};

// ***************************************************************************
// Static function declaration

/**
 * @brief Initializes the Peltier's timer
 */
static void init_peltier_timer();

static void init_fan_timer();

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
        init_fan_timer();

        hardware.initialized = true;

        thermal_hardware_set_fan_power(0);
        thermal_hardware_disable_peltiers();
        thermal_hardware_set_eeprom_write_protect(true);
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
    
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, HEATING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, COOLING_CHANNEL, 0);

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
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, COOLING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, HEATING_CHANNEL, pwm);
    
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
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, HEATING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, COOLING_CHANNEL, pwm);
    
    hardware.hot_side_power = 0.0F;
    hardware.cool_side_power = power;

    return true;
}

bool thermal_hardware_set_fan_power(double power) {
    if(!hardware.initialized) {
        return false;
    }
    if(power > 1.0F) {
        return false;
    }
    uint32_t pwm = power * (double)FAN_MAX_PWM;
    
    // The fan controller will default to full power if it thinks the
    // control line is disconnected, and unfortunately it thinks a 0% PWM
    // is a disconnection. So the lowest allowable PWM is 0.01, which 
    // still results in the fan staying still.
    if(pwm == 0) { pwm = 1; }
    __HAL_TIM_SET_COMPARE(&hardware.fan_timer, FAN_CHANNEL, pwm);
    return true;
}

void thermal_hardware_set_eeprom_write_protect(bool set) {
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN,
        set ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

// ***************************************************************************
// Static function implementation

static void init_peltier_timer() {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    hardware.peltier_timer.State = HAL_TIM_STATE_RESET;
    hardware.peltier_timer.Instance = TIM2;
    hardware.peltier_timer.Init.Prescaler = TIM2_PRESCALER;
    hardware.peltier_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    hardware.peltier_timer.Init.Period = TIM2_RELOAD;
    hardware.peltier_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    hardware.peltier_timer.Init.RepetitionCounter = 0;
    hardware.peltier_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_PWM_Init(&hardware.peltier_timer);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(&hardware.peltier_timer, &sMasterConfig);
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

    hal_ret = HAL_TIM_PWM_ConfigChannel(&hardware.peltier_timer,
                                        &sConfigOC,
                                        HEATING_CHANNEL);
    configASSERT(hal_ret == HAL_OK);

    hal_ret = HAL_TIM_PWM_ConfigChannel(&hardware.peltier_timer,
                                        &sConfigOC,
                                        COOLING_CHANNEL);
    configASSERT(hal_ret == HAL_OK);
    
    // Set up the PWM GPIO pins 

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**TIM2 GPIO Configuration
    PB10    ------> TIM2_CH3
    PB11    ------> TIM2_CH4
    */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    GPIO_InitStruct.Pin = HEATING_PIN;
    HAL_GPIO_Init(HEATING_PORT, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = COOLING_PIN;
    HAL_GPIO_Init(COOLING_PORT, &GPIO_InitStruct);

    // Activate both PWM channels with a compare val of 0
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, HEATING_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(&hardware.peltier_timer, COOLING_CHANNEL, 0);
    HAL_TIM_PWM_Start(&hardware.peltier_timer, HEATING_CHANNEL);
    HAL_TIM_PWM_Start(&hardware.peltier_timer, COOLING_CHANNEL);
}

static void init_fan_timer() {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};


    // Configure timer 16 for PWMN control on channel 1
    hardware.fan_timer.State = HAL_TIM_STATE_RESET;
    hardware.fan_timer.Instance = TIM16;
    hardware.fan_timer.Init.Prescaler = TIM16_PRESCALER;
    hardware.fan_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    hardware.fan_timer.Init.Period = TIM16_RELOAD;
    hardware.fan_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    hardware.fan_timer.Init.RepetitionCounter = 0;
    hardware.fan_timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_Base_Init(&hardware.fan_timer);
    configASSERT(hal_ret == HAL_OK);
    hal_ret = HAL_TIM_PWM_Init(&hardware.fan_timer);
    configASSERT(hal_ret == HAL_OK);
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    hal_ret = HAL_TIM_PWM_ConfigChannel(&hardware.fan_timer, &sConfigOC, FAN_CHANNEL);
    configASSERT(hal_ret == HAL_OK);
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(&hardware.fan_timer, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);

    __GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = FAN_PWM_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM16;
    HAL_GPIO_Init(FAN_PWM_GPIO_Port, &GPIO_InitStruct);

    (void)HAL_TIM_PWM_Start(&hardware.fan_timer, FAN_CHANNEL);
}

static void init_gpio() {
    GPIO_InitTypeDef init = {0};

    __GPIOA_CLK_ENABLE();
    __GPIOB_CLK_ENABLE();
    __GPIOC_CLK_ENABLE();

    init.Pin = PELTIER_ENABLE_PIN;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_LOW;

    HAL_GPIO_Init(PELTIER_ENABLE_PORT, &init);

    init.Pin = EEPROM_WP_PIN;
    HAL_GPIO_Init(EEPROM_WP_PORT, &init);
}
