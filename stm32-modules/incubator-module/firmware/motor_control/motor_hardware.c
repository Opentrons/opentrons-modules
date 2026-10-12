#include "stm32g4xx_hal.h"
#include "stm32g4xx_it.h"
#include "firmware/motor_hardware.h"
#include "main.h"
#include "firmware/proximity_sensor.h"
#include "systemwide.h"
#include "FreeRTOS.h"
#include "task.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Step-timer interrupt rate. Matches motor_interrupt_controller::TIMER_FREQ.
#define MOTOR_INTERRUPT_FREQ (100000)
/** Frequency of the driving clock is 170MHz.*/
#define TIM_APB_FREQ (170000000)
/** Preload for APB to give a 10MHz clock.*/
#define TIM_PRELOAD (16)
/** Calculated TIM period.*/
#define TIM_PERIOD (((TIM_APB_FREQ/(TIM_PRELOAD + 1)) / MOTOR_INTERRUPT_FREQ) - 1)
/** TMC5160 needs STEP high for ~100 ns; back-to-back writes at 170 MHz are shorter. */
#define STEP_PULSE_HOLD_CYCLES (64U)

typedef struct PinConfig {
    void* port;
    uint16_t pin;
    uint8_t active_setting;
} PinConfig;

typedef struct stepper_hardware_struct {
    TIM_HandleTypeDef timer;
    PinConfig enable;
    PinConfig direction;
    PinConfig step;
    proximity_sensor_id_t home_sensor;
} stepper_hardware_t;

typedef struct motor_hardware_struct {
    bool initialized;
    stepper_hardware_t motor_r;
    stepper_hardware_t motor_z;
    stepper_hardware_t motor_p;
} motor_hardware_t;

static motor_hardware_t _motor_hardware = {
    .initialized = false,
    .motor_r = {
        .timer = {0},
        .enable = {R_EN_PORT, R_EN_PIN, GPIO_PIN_SET},
        .direction = {R_DIR_PORT, R_DIR_PIN, GPIO_PIN_SET},
        .step = {R_STEP_PORT, R_STEP_PIN, GPIO_PIN_SET},
        .home_sensor = PROXIMITY_SENSOR_PC2,
    },
    .motor_z = {
        .timer = {0},
        .enable = {Z_EN_PORT, Z_EN_PIN, GPIO_PIN_SET},
        .direction = {Z_DIR_PORT, Z_DIR_PIN, GPIO_PIN_SET},
        .step = {Z_STEP_PORT, Z_STEP_PIN, GPIO_PIN_SET},
        .home_sensor = PROXIMITY_SENSOR_PC2,
    },
    .motor_p = {
        .timer = {0},
        .enable = {P_EN_PORT, P_EN_PIN, GPIO_PIN_SET},
        .direction = {P_DIR_PORT, P_DIR_PIN, GPIO_PIN_SET},
        .step = {P_STEP_PORT, P_STEP_PIN, GPIO_PIN_SET},
        .home_sensor = PROXIMITY_SENSOR_PC2,
    },
};

void motor_hardware_gpio_init(void){
    GPIO_InitTypeDef init = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /*Configure GPIO pins : OUTPUTS */
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;

    // R MOTOR
    init.Pin = R_EN_PIN;
    HAL_GPIO_Init(R_EN_PORT, &init);

    init.Pin = R_DIR_PIN;
    HAL_GPIO_Init(R_DIR_PORT, &init);

    HAL_GPIO_WritePin(R_STEP_PORT, R_STEP_PIN, GPIO_PIN_RESET);
    init.Pin = R_STEP_PIN;
    HAL_GPIO_Init(R_STEP_PORT, &init);

    // Z MOTOR
    init.Pin = Z_EN_PIN;
    HAL_GPIO_Init(Z_EN_PORT, &init);

    init.Pin = Z_DIR_PIN;
    HAL_GPIO_Init(Z_DIR_PORT, &init);

    HAL_GPIO_WritePin(Z_STEP_PORT, Z_STEP_PIN, GPIO_PIN_RESET);
    init.Pin = Z_STEP_PIN;
    HAL_GPIO_Init(Z_STEP_PORT, &init);

    // Platform MOTOR
    init.Pin = P_EN_PIN;
    HAL_GPIO_Init(P_EN_PORT, &init);

    init.Pin = P_DIR_PIN;
    HAL_GPIO_Init(P_DIR_PORT, &init);

    HAL_GPIO_WritePin(P_STEP_PORT, P_STEP_PIN, GPIO_PIN_RESET);
    init.Pin = P_STEP_PIN;
    HAL_GPIO_Init(P_STEP_PORT, &init);


    /* USB_VBUS_MCU (PB4). Nothing else drives this pin; hold it low. */
    HAL_GPIO_WritePin(USB_VBUS_MCU_GPIO_Port, USB_VBUS_MCU_Pin, GPIO_PIN_RESET);
    init.Pin = USB_VBUS_MCU_Pin;
    HAL_GPIO_Init(USB_VBUS_MCU_GPIO_Port, &init);
}

// R Motor timer
void tim1_init(TIM_HandleTypeDef* htim) {
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    HAL_StatusTypeDef hal_ret;

    htim->Instance = TIM1;
    htim->Init.Prescaler = TIM_PRELOAD;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = TIM_PERIOD;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    hal_ret = HAL_TIM_Base_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    hal_ret = HAL_TIM_ConfigClockSource(htim, &sClockSourceConfig);
    configASSERT(hal_ret == HAL_OK);

    hal_ret = HAL_TIM_PWM_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(htim, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    hal_ret = HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_1);
    configASSERT(hal_ret == HAL_OK);

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
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(htim, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);

    HAL_NVIC_SetPriority(TIM1_UP_TIM16_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
}

// Z motor timer
void tim20_init(TIM_HandleTypeDef* htim) {
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    HAL_StatusTypeDef hal_ret;

    htim->Instance = TIM20;
    htim->Init.Prescaler = TIM_PRELOAD;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = TIM_PERIOD;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    hal_ret = HAL_TIM_Base_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    hal_ret = HAL_TIM_ConfigClockSource(htim, &sClockSourceConfig);
    configASSERT(hal_ret == HAL_OK);

    hal_ret = HAL_TIM_PWM_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(htim, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    hal_ret = HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_1);
    configASSERT(hal_ret == HAL_OK);

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
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(htim, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);

    HAL_NVIC_SetPriority(TIM20_UP_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM20_UP_IRQn);
}

// P motor timer
void tim8_init(TIM_HandleTypeDef* htim) {
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    HAL_StatusTypeDef hal_ret;

    htim->Instance = TIM8;
    htim->Init.Prescaler = TIM_PRELOAD;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = TIM_PERIOD;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    hal_ret = HAL_TIM_Base_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    hal_ret = HAL_TIM_ConfigClockSource(htim, &sClockSourceConfig);
    configASSERT(hal_ret == HAL_OK);

    hal_ret = HAL_TIM_PWM_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(htim, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    hal_ret = HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_1);
    configASSERT(hal_ret == HAL_OK);

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
    hal_ret = HAL_TIMEx_ConfigBreakDeadTime(htim, &sBreakDeadTimeConfig);
    configASSERT(hal_ret == HAL_OK);

    HAL_NVIC_SetPriority(TIM8_UP_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM8_UP_IRQn);
}



void motor_hardware_init(void){
    if (!_motor_hardware.initialized) {
        motor_hardware_gpio_init();
        tim1_init(&_motor_hardware.motor_r.timer);
        tim20_init(&_motor_hardware.motor_z.timer);
        tim8_init(&_motor_hardware.motor_p.timer);
    }
    _motor_hardware.initialized = true;
}

static stepper_hardware_t* lookup_motor(MotorID motor_id) {
    if (motor_id == MOTOR_R) {
        return &_motor_hardware.motor_r;
    }
    if (motor_id == MOTOR_Z) {
        return &_motor_hardware.motor_z;
    }
    if (motor_id == MOTOR_P) {
        return &_motor_hardware.motor_p;
    }
    return NULL;
}

static uint8_t invert_gpio_value(uint8_t setting) {
    return setting == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

void set_pin(PinConfig config) {
    HAL_GPIO_WritePin((GPIO_TypeDef*)config.port, config.pin,
                      config.active_setting);
}

void reset_pin(PinConfig config) {
    HAL_GPIO_WritePin((GPIO_TypeDef*)config.port, config.pin,
                      invert_gpio_value(config.active_setting));
}

bool hw_enable_motor(MotorID motor_id) {
    stepper_hardware_t* motor = lookup_motor(motor_id);
    if (motor == NULL) {
        return false;
    }
    set_pin(motor->enable);
    return true;
}

void hw_start_motor_timer(MotorID motor_id) {
    stepper_hardware_t* motor = lookup_motor(motor_id);
    if (motor == NULL) {
        return;
    }
    HAL_TIM_Base_Start_IT(&motor->timer);
}

bool hw_disable_motor(MotorID motor_id) {
    stepper_hardware_t* motor = lookup_motor(motor_id);
    if (motor == NULL) {
        return false;
    }
    reset_pin(motor->enable);
    return true;
}

bool hw_stop_motor(MotorID motor_id) {
    stepper_hardware_t* motor = lookup_motor(motor_id);
    if (motor == NULL) {
        return false;
    }
    HAL_StatusTypeDef status = HAL_TIM_Base_Stop_IT(&motor->timer);
    return status == HAL_OK;
}

void hw_step_motor(MotorID motor_id) {
    stepper_hardware_t* motor = lookup_motor(motor_id);
    if (motor == NULL) {
        return;
    }
    set_pin(motor->step);
    for (volatile uint32_t i = 0; i < STEP_PULSE_HOLD_CYCLES; ++i) {
    }
    reset_pin(motor->step);
}

void hw_set_direction(MotorID motor_id, bool direction) {
    stepper_hardware_t* motor = lookup_motor(motor_id);
    if (motor == NULL) {
        return;
    }
    if (direction) {
        set_pin(motor->direction);
    } else {
        reset_pin(motor->direction);
    }
}

/* Reads the pin directly: the debounced state takes a task-level critical
 * section, and this runs from the step-timer interrupt. */
bool hw_read_home_sensor(MotorID motor_id) {
    stepper_hardware_t* motor = lookup_motor(motor_id);
    if (motor == NULL) {
        return false;
    }
    return !proximity_sensor_read_raw(motor->home_sensor);
}

void TIM1_UP_TIM16_IRQHandler(void) {
    HAL_TIM_IRQHandler(&_motor_hardware.motor_r.timer);
}

void TIM20_UP_IRQHandler(void) {
    HAL_TIM_IRQHandler(&_motor_hardware.motor_z.timer);
}

void TIM8_UP_IRQHandler(void) {
    HAL_TIM_IRQHandler(&_motor_hardware.motor_p.timer);
}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
