#include "firmware/thermal_fan_hardware.h"


#include "FreeRTOS.h"
#include "stm32g4xx_hal_def.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"
#include "task.h"

// Private definitions
#define SINK_FAN_PWM_Pin (GPIO_PIN_6)
#define SINK_FAN_PWM_GPIO_Port (GPIOA)

#define PULSE_WIDTH_FREQ (25000)
#define TIMER_CLOCK_FREQ (170000000)
// These two together give a 25kHz pulse width, and the ARR value
// of 99 gives us a nice scale of 0-100 for the pulse width.
// A finer scale is possible by reducing the prescale value and
// adjusting the reload to match.
#define TIM16_PRESCALER (67)
//#define TIM1_RELOAD    (99)
#define TIM16_RELOAD ((TIMER_CLOCK_FREQ / (PULSE_WIDTH_FREQ * (TIM16_PRESCALER + 1))) - 1)
// PWM should be scaled from 0 to MAX_PWM, inclusive
#define MAX_PWM (TIM16_RELOAD + 1)

// Private typedefs

struct Fans {
    // Configuration
    GPIO_TypeDef *enable_port;
    const uint16_t enable_pin;
    const uint16_t pwm_channel;
    // Current status
    bool initialized;
    double power;
    TIM_HandleTypeDef timer;
};

// Local variables

static struct Fans _fans = {
    .enable_port = GPIOD,
    .enable_pin  = GPIO_PIN_1,
    .pwm_channel = TIM_CHANNEL_1,
    .initialized = false,
    .power = 0.0F,
    .timer = {}
};

// Private function declarations

/**
 * @brief Enable or disable the fans (12v power supply)
 * 
 * @param[in] enabled True to enable the fans, false to turn them off
 * @return True on success, false if \p enabled could not be written
 */
static bool thermal_fan_set_enable(bool enabled);

// Public function implementations

void thermal_fan_initialize(void) {
    HAL_StatusTypeDef hal_ret = HAL_ERROR;
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __GPIOA_CLK_ENABLE();
    __GPIOD_CLK_ENABLE();

    // Disable the 12v converter first
    GPIO_InitStruct.Pin = _fans.enable_pin;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(_fans.enable_port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(_fans.enable_port, _fans.enable_pin, GPIO_PIN_RESET);

    // Configure timer 16 for PWMN control on channel 1
    _fans.timer.Instance = TIM16;
    _fans.timer.Init.Prescaler = TIM16_PRESCALER;
    _fans.timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    _fans.timer.Init.Period = TIM16_RELOAD;
    _fans.timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    _fans.timer.Init.RepetitionCounter = 0;
    _fans.timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_Base_Init(&_fans.timer);
    configASSERT(hal_ret == HAL_OK);
    hal_ret = HAL_TIM_PWM_Init(&_fans.timer);
    configASSERT(hal_ret == HAL_OK);
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    hal_ret = HAL_TIM_PWM_ConfigChannel(&_fans.timer, &sConfigOC, _fans.pwm_channel);
    configASSERT(hal_ret == HAL_OK);
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(&_fans.timer, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);

    // This is a copy+paste of the HAL_TIM_MspPostInit implementation because
    // there's no actual reason for it to live in that function
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**TIM16 GPIO Configuration
    PA6     ------> TIM16_CH1
    */
    GPIO_InitStruct.Pin = SINK_FAN_PWM_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM16;
    HAL_GPIO_Init(SINK_FAN_PWM_GPIO_Port, &GPIO_InitStruct);

    _fans.initialized = true;
}


bool thermal_fan_set_power(double power) {
    if(!_fans.initialized) { return false; }
    if(power > 1.0F) { power = 1.0F; }
    if(power < 0.0F) { power = 0.0F; }

    double old_power = _fans.power;
    _fans.power = power;

    if(power == 0.0F) {
        if(!thermal_fan_set_enable(false)) { return false; }
        return HAL_TIM_PWM_Stop(&_fans.timer, _fans.pwm_channel) == HAL_OK;
    }

    uint32_t pwm = _fans.power * (double)MAX_PWM;
    if(!thermal_fan_set_enable(true)) { return false; }
    __HAL_TIM_SET_COMPARE(&_fans.timer, _fans.pwm_channel, pwm);
    // PWM_Start will fail if we call it twice, so make sure the fan is 
    // off before calling it
    if(old_power == 0.0F) {
        if(HAL_TIM_PWM_Start(&_fans.timer, _fans.pwm_channel) != HAL_OK) {
            // Disable 12v if we couldn't set the PWM
            thermal_fan_set_enable(false);
            return false;
        }
    }
    

    return true;
}

// Local function implementations

static bool thermal_fan_set_enable(bool enabled) {
    if(!_fans.initialized) { return false; }

    GPIO_PinState state = (enabled) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(_fans.enable_port, _fans.enable_pin, state);

    return true;
}
