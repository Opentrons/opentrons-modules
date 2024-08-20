
#include "stm32g4xx_hal.h"
#include "stm32g4xx_it.h"
#include "systemwide.h"

#include "FreeRTOS.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


/******************* Motor Z *******************/
/** Motor hardware **/
#define Z_STEP_PIN (GPIO_PIN_2)
#define Z_STEP_PORT (GPIOC)
#define Z_DIR_PIN (GPIO_PIN_1)
#define Z_DIR_PORT (GPIOC)
#define Z_EN_PIN (GPIO_PIN_3)
#define Z_EN_PORT (GPIOA)
#define Z_N_BRAKE_PIN (GPIO_PIN_7)
#define Z_N_BRAKE_PORT (GPIOB)

/** Limit switches **/
/* Note: Photointerrupters limit switches */
#define Z_MINUS_LIMIT_PIN (GPIO_PIN_3)
#define Z_MINUS_LIMIT_PORT (GPIOC)
#define Z_PLUS_LIMIT_PIN (GPIO_PIN_0)
#define Z_PLUS_LIMIT_PORT (GPIOA)


/******************* Motor X *******************/

/** Motor hardware **/
#define X_STEP_PIN (GPIO_PIN_7)
#define X_STEP_PORT (GPIOA)
#define X_DIR_PIN (GPIO_PIN_6)
#define X_DIR_PORT (GPIOA)
#define X_EN_PIN (GPIO_PIN_4)
#define X_EN_PORT (GPIOA)
#define X_N_BRAKE_PIN (GPIO_PIN_9)
#define X_N_BRAKE_PORT (GPIOB)


/** Limit switches **/
/* Note: Photointerrupters limit switches */
#define X_MINUS_LIMIT_PIN (GPIO_PIN_1)
#define X_MINUS_LIMIT_PORT (GPIOA)
#define X_PLUS_LIMIT_PIN (GPIO_PIN_2)
#define X_PLUS_LIMIT_PORT (GPIOA)


/******************* Motor L *******************/

/** Motor hardware **/
#define L_STEP_PIN (GPIO_PIN_1)
#define L_STEP_PORT (GPIOB)
#define L_DIR_PIN (GPIO_PIN_0)
#define L_DIR_PORT (GPIOB)
#define L_EN_PIN (GPIO_PIN_5)
#define L_EN_PORT (GPIOC)


/** Limit switches **/
/* Note: Mechanical limit switches */
#define L_N_HELD_PIN (GPIO_PIN_5)
#define L_N_HELD_PORT (GPIOB)
#define L_N_RELEASED_PIN (GPIO_PIN_11)
#define L_N_RELEASED_PORT (GPIOC)

/**************** COMMON ********************/
#define ESTOP_PIN GPIO_PIN_6
#define ESTOP_PORT GPIOB


// Frequency of the motor interrupt callbacks is 300kHz, providing some extra
// overhead over the velocities used by this application.
#define MOTOR_INTERRUPT_FREQ (100000)
/** Frequency of the driving clock is 170MHz.*/
#define TIM_APB_FREQ (170000000)
/** Preload for APB to give a 10MHz clock.*/
#define TIM_PRELOAD (16)
/** Calculated TIM period.*/
#define TIM_PERIOD (((TIM_APB_FREQ/(TIM_PRELOAD + 1)) / MOTOR_INTERRUPT_FREQ) - 1)

TIM_HandleTypeDef htim17;
TIM_HandleTypeDef htim20;
TIM_HandleTypeDef htim3;

typedef struct stepper_hardware_struct {
    bool enabled;
    bool moving;
    bool direction;
    int32_t step_count;
    int32_t step_target;
    TIM_HandleTypeDef timer;
} stepper_hardware_t;

typedef struct motor_hardware_struct {
    bool initialized;
    stepper_hardware_t motor_x;
    stepper_hardware_t motor_z;
    stepper_hardware_t motor_l;
} motor_hardware_t;


static motor_hardware_t _motor_hardware = {
    .initialized = false,
    .motor_x = {
        .enabled = false,
        .moving = false,
        .direction = false,
        .timer = {0},
    },
    .motor_z = {
        .enabled = false,
        .moving = false,
        .direction = false,
        .timer = {0}
    },
    .motor_l = {
        .enabled = false,
        .moving = false,
        .direction = false,
        .timer = {0}
    }
};


void motor_hardware_gpio_init(void){

    GPIO_InitTypeDef init = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pins : OUTPUTS */
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;

    // Z MOTOR
    init.Pin = Z_EN_PIN;
    HAL_GPIO_Init(Z_EN_PORT, &init);

    init.Pin = Z_DIR_PIN;
    HAL_GPIO_Init(Z_DIR_PORT, &init);

    init.Pin = Z_N_BRAKE_PIN;
    HAL_GPIO_Init(Z_N_BRAKE_PORT, &init);

    init.Pin  = Z_STEP_PIN;
    HAL_GPIO_Init(Z_STEP_PORT, &init);

    // X MOTOR
    init.Pin = X_EN_PIN;
    HAL_GPIO_Init(X_EN_PORT, &init);

    init.Pin = X_DIR_PIN;
    HAL_GPIO_Init(X_DIR_PORT, &init);

    init.Pin = X_STEP_PIN;
    HAL_GPIO_Init(X_STEP_PORT, &init);

    // L MOTOR
    init.Pin = L_EN_PIN;
    HAL_GPIO_Init(L_EN_PORT, &init);

    init.Pin = L_DIR_PIN;
    HAL_GPIO_Init(L_DIR_PORT, &init);

    init.Pin = L_STEP_PIN;
    HAL_GPIO_Init(L_STEP_PORT, &init);

    /*Configure GPIO pins : INPUTS */
    init.Mode = GPIO_MODE_INPUT;

    // Z MOTOR
    init.Pin = Z_MINUS_LIMIT_PIN;
    HAL_GPIO_Init(Z_MINUS_LIMIT_PORT, &init);

    init.Pin = Z_PLUS_LIMIT_PIN;
    HAL_GPIO_Init(Z_PLUS_LIMIT_PORT, &init);

    // X MOTOR
    init.Pin = X_MINUS_LIMIT_PIN;
    HAL_GPIO_Init(X_MINUS_LIMIT_PORT, &init);

    init.Pin = X_PLUS_LIMIT_PIN;
    HAL_GPIO_Init(X_PLUS_LIMIT_PORT, &init);

    // L MOTOR
    init.Pin = L_N_HELD_PIN;
    HAL_GPIO_Init(L_N_HELD_PORT, &init);

    init.Pin = L_N_RELEASED_PIN;
    HAL_GPIO_Init(L_N_RELEASED_PORT, &init);

    init.Pin = ESTOP_PIN;
    HAL_GPIO_Init(ESTOP_PORT, &init);
}

// X motor timer
void tim17_init(TIM_HandleTypeDef* htim) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    HAL_StatusTypeDef hal_ret;

    htim->Instance = TIM17;
    htim->Init.Prescaler = TIM_PRELOAD;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = TIM_PERIOD;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    hal_ret = HAL_TIM_Base_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(htim, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    HAL_NVIC_SetPriority(TIM20_UP_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM20_UP_IRQn);
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
    hal_ret = HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_2);
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

    HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM17_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM17_IRQn);
}

// L motor timer
void tim3_init(TIM_HandleTypeDef* htim) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    HAL_StatusTypeDef hal_ret;

    htim->Instance = TIM3;
    htim->Init.Prescaler = TIM_PRELOAD;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = TIM_PERIOD;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    hal_ret = HAL_TIM_Base_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(htim, &sMasterConfig);
    configASSERT(hal_ret == HAL_OK);

    HAL_NVIC_SetPriority(TIM3_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

void motor_hardware_init(void){
    if (!_motor_hardware.initialized) {
        motor_hardware_gpio_init();
        tim17_init(&_motor_hardware.motor_x.timer);
        tim20_init(&_motor_hardware.motor_z.timer);
        tim3_init(&_motor_hardware.motor_l.timer);
    }
    _motor_hardware.initialized = true;
}

void hw_enable_ebrake(MotorID motor_id, bool enable) {
    if (motor_id != MOTOR_Z) {
        return;
    }
    HAL_GPIO_WritePin(Z_N_BRAKE_PORT, Z_N_BRAKE_PIN, enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return;
}

bool hw_enable_motor(MotorID motor_id) {
    hw_enable_ebrake(motor_id, false);
    void* port;
    uint16_t pin;
    HAL_StatusTypeDef     status = HAL_OK;
    switch (motor_id) {
        case MOTOR_Z:
            port = Z_EN_PORT;
            pin = Z_EN_PIN;
            status = HAL_TIM_Base_Start_IT(&_motor_hardware.motor_z.timer);
            break;
        case MOTOR_X:
            port = X_EN_PORT;
            pin = X_EN_PIN;
            status = HAL_TIM_Base_Start_IT(&_motor_hardware.motor_x.timer);
            break;
        case MOTOR_L:
            port = L_EN_PORT;
            pin = L_EN_PIN;
            status = HAL_TIM_Base_Start_IT(&_motor_hardware.motor_l.timer);

            break;
        default:
            return status == HAL_OK;
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    return status == HAL_OK;
}

bool hw_disable_motor(MotorID motor_id) {
    void* port;
    uint16_t pin;
    HAL_StatusTypeDef     status = HAL_OK;
    switch (motor_id) {
        case MOTOR_Z:
            __HAL_TIM_DISABLE_IT(&_motor_hardware.motor_z.timer, TIM_IT_UPDATE);
            port = Z_EN_PORT;
            pin = Z_EN_PIN;
            status = HAL_TIM_Base_Stop_IT(&_motor_hardware.motor_z.timer);
            break;
        case MOTOR_X:
            __HAL_TIM_DISABLE_IT(&_motor_hardware.motor_x.timer, TIM_IT_UPDATE);
            port = X_EN_PORT;
            pin = X_EN_PIN;
            status = HAL_TIM_Base_Stop_IT(&_motor_hardware.motor_x.timer);
            break;
        case MOTOR_L:
            __HAL_TIM_DISABLE_IT(&_motor_hardware.motor_l.timer, TIM_IT_UPDATE);
            port = L_EN_PORT;
            pin = L_EN_PIN;
            status = HAL_TIM_Base_Stop_IT(&_motor_hardware.motor_l.timer);
            break;
        default:
            return status == HAL_OK;
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    hw_enable_ebrake(motor_id, true);
    return status == HAL_OK;
}

void step_motor(MotorID motor_id) {
    void* port;
    uint16_t pin;
    switch (motor_id) {
        case MOTOR_Z:
            port = Z_STEP_PORT;
            pin = Z_STEP_PIN;
            break;
        case MOTOR_X:
            port = X_STEP_PORT;
            pin = X_STEP_PIN;
            break;
        case MOTOR_L:
            port = L_STEP_PORT;
            pin = L_STEP_PIN;
            break;
        default:
            return;
    }
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);

}

void hw_set_direction(MotorID motor_id, bool direction) {
    void* port;
    uint16_t pin;
    switch (motor_id) {
        case MOTOR_Z:
            port = Z_DIR_PORT;
            pin = Z_DIR_PIN;
            break;
        case MOTOR_X:
            port = X_DIR_PORT;
            pin = X_DIR_PIN;
            break;
        case MOTOR_L:
            port = L_DIR_PORT;
            pin = L_DIR_PIN;
            break;
        default:
            return;
    }
    HAL_GPIO_WritePin(port, pin, direction ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void TIM3_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&_motor_hardware.motor_l.timer);
}

void TIM20_UP_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&_motor_hardware.motor_z.timer);
}

void TIM1_TRG_COM_TIM17_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&_motor_hardware.motor_x.timer);
}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
