/**
 * @file motor_hardware.c
 * @brief Implements motor hardware control code
 */
#include "firmware/motor_hardware.h"

#include <string.h>  // for memset
#include <stdlib.h> // for abs

#include "FreeRTOS.h"
#include "task.h"

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_tim.h"

#include "firmware/motor_spi_hardware.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// ----------------------------------------------------------------------------
// Local definitions

#define SOLENOID_Port GPIOD
#define SOLENOID_Pin GPIO_PIN_8
#define LID_STEPPER_ENABLE_Port GPIOE
#define LID_STEPPER_RESET_Pin GPIO_PIN_12
#define LID_STEPPER_ENABLE_Pin GPIO_PIN_10
#define LID_STEPPER_FAULT_Pin GPIO_PIN_11
#define LID_STEPPER_CONTROL_Port GPIOA
#define LID_STEPPER_VREF_Pin GPIO_PIN_4
#define LID_STEPPER_DIR_Pin GPIO_PIN_1
#define LID_STEPPER_STEP_Pin GPIO_PIN_0
#define LID_STEPPER_VREF_CHANNEL DAC_CHANNEL_1
#define LID_STEPPER_STEP_Channel TIM_CHANNEL_1

/** Port for the lid closed optical switch.*/
#define LID_CLOSED_SWITCH_PORT (GPIOD)
/** Pin for the lid closed optical switch.*/
#define LID_CLOSED_SWITCH_PIN (GPIO_PIN_9)

/** Port for the lid open optical switch.*/
#define LID_OPEN_SWITCH_PORT (GPIOB)
/** Pin for the lid open optical switch.*/
#define LID_OPEN_SWITCH_PIN (GPIO_PIN_7)

/** Port for the Photointerrupt Enable line.*/
#define PHOTOINTERRUPT_ENABLE_PORT (GPIOE)
/** Pin for the photointerrupt enable line.*/
#define PHOTOINTERRUPT_ENABLE_PIN (GPIO_PIN_0)

/** Port for the step pulse pin.*/
#define SEAL_STEPPER_STEP_PORT (GPIOB)
/** Pin for the step pulse pin.*/
#define SEAL_STEPPER_STEP_PIN (GPIO_PIN_10)
/** Port for the step direction port.*/
#define SEAL_STEPPER_DIRECTION_PORT (GPIOB)
/** Pin for the step direction pin.*/
#define SEAL_STEPPER_DIRECTION_PIN (GPIO_PIN_11)
/** Port for the enable pin.*/
#define SEAL_STEPPER_ENABLE_PORT (GPIOE)
/** Pin for enable pin.*/
#define SEAL_STEPPER_ENABLE_PIN (GPIO_PIN_15)
/** Diag0 pin is used for error signals.*/
#define SEAL_STEPPER_DIAG0_PORT (GPIOD)
/** Diag0 pin is used for error signals.*/
#define SEAL_STEPPER_DIAG0_PIN (GPIO_PIN_3)
/** IRQ for the seal stepper diag0.*/
#define SEAL_STEPPER_DIAG0_IRQ (EXTI3_IRQn)
/** Diag1 pin is used for error signals.*/
#define SEAL_STEPPER_DIAG1_PORT (GPIOD)
/** Diag1 pin is used for error signals.*/
#define SEAL_STEPPER_DIAG1_PIN (GPIO_PIN_4)
/** IRQ for the seal stepper diag0.*/
#define SEAL_STEPPER_DIAG1_IRQ (EXTI4_IRQn)

/** Frequency of the driving clock for TIM6 is 170MHz.*/
#define TIM6_APB_FREQ (170000000)
/** Preload for APB to give a 10MHz clock.*/
#define TIM6_PRELOAD (16)
/** Calculated TIM6 period.*/
#define TIM6_PERIOD (((TIM6_APB_FREQ/(TIM6_PRELOAD + 1)) / MOTOR_INTERRUPT_FREQ) - 1)

// ----------------------------------------------------------------------------
// Local typedefs

typedef struct lid_hardware_struct {
    // Whether the lid motor is moving
    bool moving;
    // Direction the lid is moving
    bool direction;
    // Current step count for the lid
    int32_t step_count;
    // Target step count for the lid
    int32_t step_target;
    // Timer for the lid motor
    TIM_HandleTypeDef timer;
    // DAC for lid current control
    DAC_HandleTypeDef dac;
} lid_hardware_t;

typedef struct seal_hardware_struct {
    // Whether the seal motor driver is enabled
    bool enabled;
    // Is there a movement in progress?
    bool moving;
    // Current direction of the seal stepper.
    // True = forwards, False = backwards
    bool direction;
    // Timer handle for the seal stepper
    TIM_HandleTypeDef timer;
} seal_hardware_t;

typedef struct motor_hardware_struct {
    // Whether this driver has been initialized
    bool initialized;
    // Callback for completion of a lid stepper movement
    motor_hardware_callbacks callbacks;
    // Current status of the lid stepper
    lid_hardware_t lid_stepper;
    // Encapsulates seal motor information
    seal_hardware_t seal;
} motor_hardware_t;

// Local variables

static motor_hardware_t _motor_hardware = {
    .initialized = false,
    .callbacks = {
        .lid_stepper_complete = NULL,
        .seal_stepper_tick = NULL
    },
    .lid_stepper = {
        .moving = false,
        .direction = true,
        .step_count = 0,
        .step_target = 0,
        .timer = {0},
        .dac = {0}
    },
    .seal = {
        .enabled = false,
        .moving = false,
        .direction = false,
        .timer = {0}
    }
};

// ----------------------------------------------------------------------------
// Local function declaration

static void init_motor_gpio(void);
void HAL_TIM_OC_MspInit(TIM_HandleTypeDef* htim);
static void init_dac1(DAC_HandleTypeDef* hdac);
static void init_tim2(TIM_HandleTypeDef* htim);
static void init_tim6(TIM_HandleTypeDef* htim);

// ----------------------------------------------------------------------------
// Public function implementation

void motor_hardware_setup(const motor_hardware_callbacks* callbacks) {
    configASSERT(callbacks != NULL);
    configASSERT(callbacks->lid_stepper_complete != NULL);
    configASSERT(callbacks->seal_stepper_tick != NULL);
    configASSERT(callbacks->seal_stepper_error != NULL);

    memcpy(&_motor_hardware.callbacks, callbacks, sizeof(_motor_hardware.callbacks));

    if(!_motor_hardware.initialized) {
        init_motor_gpio();
        init_dac1(&_motor_hardware.lid_stepper.dac);
        init_tim2(&_motor_hardware.lid_stepper.timer);
        init_tim6(&_motor_hardware.seal.timer);
        motor_spi_initialize();
    }

    motor_hardware_lid_stepper_reset();
    motor_hardware_set_seal_enable(false);

    _motor_hardware.initialized = true;
}

//PA0/PA1/PB10/PB11 = TIM2CH1/2/3/4 (all GPIO_AF1_TIM2)
//control via increment_step in motor_task.hpp?
//handle end switch IT start and stop
void motor_hardware_lid_stepper_start(int32_t steps) {
    //check for fault
    _motor_hardware.lid_stepper.step_count = 0;
    // Multiply number of steps by 2 because the timer is in toggle mode (2 interrupts = 1 microstep)
    _motor_hardware.lid_stepper.step_target = abs(steps) * 2;

    // True = opening
    _motor_hardware.lid_stepper.direction = (steps > 0);
    if (steps > 0) {
        HAL_GPIO_WritePin(LID_STEPPER_CONTROL_Port, LID_STEPPER_DIR_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(LID_STEPPER_CONTROL_Port, LID_STEPPER_DIR_Pin, GPIO_PIN_RESET);
    }

    HAL_TIM_OC_Start_IT(&_motor_hardware.lid_stepper.timer, LID_STEPPER_STEP_Channel);
}

void motor_hardware_lid_stepper_stop() {
    HAL_TIM_OC_Stop_IT(&_motor_hardware.lid_stepper.timer, LID_STEPPER_STEP_Channel);
}

void motor_hardware_lid_increment() {
    bool done = false;
    _motor_hardware.lid_stepper.step_count++;
    if(_motor_hardware.lid_stepper.direction) {
        // Check if lid is open
        if(motor_hardware_lid_read_open()) {
            done = true;
        }
    } else {
        // Check if lid is closed
        if(motor_hardware_lid_read_closed()) {
            done = true;
        }
    }
    // If the lid hit a limit switch, 
    if (_motor_hardware.lid_stepper.step_count > (_motor_hardware.lid_stepper.step_target - 1)) {
        done = true;
    }

    if(done) {
        motor_hardware_lid_stepper_stop();
        _motor_hardware.callbacks.lid_stepper_complete();
    }
}

void motor_hardware_lid_stepper_set_dac(uint8_t dacval) {
    HAL_DAC_SetValue(&_motor_hardware.lid_stepper.dac, LID_STEPPER_VREF_CHANNEL, DAC_ALIGN_8B_R, dacval);
}

bool motor_hardware_lid_stepper_check_fault(void) {
    return (HAL_GPIO_ReadPin(LID_STEPPER_ENABLE_Port, LID_STEPPER_FAULT_Pin) == GPIO_PIN_RESET) ? true : false;
}

bool motor_hardware_lid_stepper_reset(void) {
    //if fault, try resetting
    if (motor_hardware_lid_stepper_check_fault()) {
        HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_RESET_Pin, GPIO_PIN_RESET);
        vTaskDelay(pdMS_TO_TICKS(100));
        HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_RESET_Pin, GPIO_PIN_SET);
    }
    //return false if fault persists
    return (motor_hardware_lid_stepper_check_fault() == true) ? false : true;
}

bool motor_hardware_lid_read_closed(void) {
    return (HAL_GPIO_ReadPin(LID_CLOSED_SWITCH_PORT, LID_CLOSED_SWITCH_PIN) == GPIO_PIN_RESET) ? true : false;
}

bool motor_hardware_lid_read_open(void) {
    return (HAL_GPIO_ReadPin(LID_OPEN_SWITCH_PORT, LID_OPEN_SWITCH_PIN) == GPIO_PIN_SET) ? true : false;
}

bool motor_hardware_set_seal_enable(bool enable) {
    // Active low
    HAL_GPIO_WritePin(SEAL_STEPPER_ENABLE_PORT, SEAL_STEPPER_ENABLE_PIN, 
        (enable) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    return true;
}

bool motor_hardware_set_seal_direction(bool direction) {
    _motor_hardware.seal.direction = direction;
    HAL_GPIO_WritePin(SEAL_STEPPER_DIRECTION_PORT, SEAL_STEPPER_DIRECTION_PIN, 
        direction ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return true;
}

bool motor_hardware_start_seal_movement(void) {
    return HAL_TIM_Base_Start_IT(&_motor_hardware.seal.timer) == HAL_OK;
}

bool motor_hardware_stop_seal_movement(void) {
    return HAL_TIM_Base_Stop_IT(&_motor_hardware.seal.timer) == HAL_OK;
}

void motor_hardware_seal_interrupt(void) {
    _motor_hardware.callbacks.seal_stepper_tick();
}

void motor_hardware_seal_step_pulse(void) {
    HAL_GPIO_WritePin(SEAL_STEPPER_STEP_PORT, SEAL_STEPPER_STEP_PIN, 
                      GPIO_PIN_SET);
    HAL_GPIO_WritePin(SEAL_STEPPER_STEP_PORT, SEAL_STEPPER_STEP_PIN, 
                      GPIO_PIN_RESET);
}

void motor_hardware_solenoid_engage() { //engage to clear/unlock sliding locking plate
    //check to confirm lid closed before engaging solenoid
    HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_SET);
    //delay to ensure engaged
}

void motor_hardware_solenoid_release() {
    HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_RESET);
    //delay to ensure disengaged
}

// ----------------------------------------------------------------------------
// Local function implementation

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void init_motor_gpio(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Enable GPIOx clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Pin = SOLENOID_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(SOLENOID_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(SOLENOID_Port, SOLENOID_Pin, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = LID_STEPPER_RESET_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(LID_STEPPER_ENABLE_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_RESET_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = LID_STEPPER_ENABLE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(LID_STEPPER_ENABLE_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(LID_STEPPER_ENABLE_Port, LID_STEPPER_ENABLE_Pin, GPIO_PIN_RESET); //enable output at init

    GPIO_InitStruct.Pin = LID_STEPPER_FAULT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LID_STEPPER_ENABLE_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LID_STEPPER_VREF_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LID_STEPPER_CONTROL_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LID_STEPPER_DIR_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LID_STEPPER_CONTROL_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LID_STEPPER_STEP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(LID_STEPPER_CONTROL_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LID_CLOSED_SWITCH_PIN;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(LID_CLOSED_SWITCH_PORT, &GPIO_InitStruct);
    
    GPIO_InitStruct.Pin = LID_OPEN_SWITCH_PIN;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(LID_OPEN_SWITCH_PORT, &GPIO_InitStruct);

    // Initialize photointerrupt to 3.3v to enable
    GPIO_InitStruct.Pin = PHOTOINTERRUPT_ENABLE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(PHOTOINTERRUPT_ENABLE_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(PHOTOINTERRUPT_ENABLE_PORT, PHOTOINTERRUPT_ENABLE_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = SEAL_STEPPER_ENABLE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = 0;
    HAL_GPIO_Init(SEAL_STEPPER_ENABLE_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SEAL_STEPPER_DIRECTION_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(SEAL_STEPPER_DIRECTION_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SEAL_STEPPER_STEP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(SEAL_STEPPER_STEP_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SEAL_STEPPER_DIAG0_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(SEAL_STEPPER_DIAG0_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = SEAL_STEPPER_DIAG1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(SEAL_STEPPER_DIAG1_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(SEAL_STEPPER_DIAG0_IRQ, 5, 0);
    HAL_NVIC_EnableIRQ(SEAL_STEPPER_DIAG0_IRQ);

    HAL_NVIC_SetPriority(SEAL_STEPPER_DIAG1_IRQ, 5, 0);
    HAL_NVIC_EnableIRQ(SEAL_STEPPER_DIAG1_IRQ);
}

void HAL_TIM_OC_MspInit(TIM_HandleTypeDef* htim) {
    if (htim == &_motor_hardware.lid_stepper.timer) {
        /* Peripheral clock enable */
        __HAL_RCC_TIM2_CLK_ENABLE();

        /* TIM2 interrupt Init */
        HAL_NVIC_SetPriority(TIM2_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    }
}

static void init_dac1(DAC_HandleTypeDef* hdac) {
    HAL_StatusTypeDef hal_ret;

    __HAL_RCC_DAC1_CLK_ENABLE();
    hdac->Instance = DAC1;
    hal_ret = HAL_DAC_Init(hdac);
    configASSERT(hal_ret == HAL_OK);

    DAC_ChannelConfTypeDef chan_config = {
        .DAC_Trigger = DAC_TRIGGER_NONE,
        .DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE,
    };
    hal_ret = HAL_DAC_ConfigChannel(hdac, &chan_config, LID_STEPPER_VREF_CHANNEL);
    configASSERT(hal_ret == HAL_OK);
    hal_ret = HAL_DAC_Start(hdac, LID_STEPPER_VREF_CHANNEL);
    configASSERT(hal_ret == HAL_OK);
    hal_ret = HAL_DAC_SetValue(hdac, LID_STEPPER_VREF_CHANNEL, DAC_ALIGN_8B_R, 0);
    configASSERT(hal_ret == HAL_OK);
}

static void init_tim2(TIM_HandleTypeDef* htim) {
    HAL_StatusTypeDef hal_ret;
    TIM_OC_InitTypeDef htim_oc;
    configASSERT(htim != NULL);
    /* Compute TIM2 clock */
    uint32_t uwTimClock = HAL_RCC_GetPCLK1Freq();
    /* Compute the prescaler value to have TIM2 counter clock equal to 1MHz */
    uint32_t uwPrescalerValue = (uint32_t) ((uwTimClock / 1000000U) - 1U);
    uint32_t uwPeriodValue = __HAL_TIM_CALC_PERIOD(uwTimClock, uwPrescalerValue, 32000); //75rpm, from TC1
    
    htim->Instance = TIM2;
    htim->Init.Prescaler = uwPrescalerValue;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = uwPeriodValue;
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    htim->Channel = HAL_TIM_ACTIVE_CHANNEL_1;
    hal_ret = HAL_TIM_OC_Init(htim);
    configASSERT(hal_ret == HAL_OK);
    
    htim_oc.OCMode = TIM_OCMODE_TOGGLE;
    htim_oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    htim_oc.OCFastMode = TIM_OCFAST_DISABLE;
    htim_oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    hal_ret = HAL_TIM_OC_ConfigChannel(htim, &htim_oc, LID_STEPPER_STEP_Channel);
    configASSERT(hal_ret == HAL_OK);
}


static void init_tim6(TIM_HandleTypeDef* htim) {
    HAL_StatusTypeDef hal_ret;
    TIM_MasterConfigTypeDef config = {0};
    configASSERT(htim != NULL);

    htim->Instance = TIM6;
    htim->Init.Prescaler = TIM6_PRELOAD;
    htim->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim->Init.Period = TIM6_PERIOD;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    hal_ret = HAL_TIM_Base_Init(htim);
    configASSERT(hal_ret == HAL_OK);

    config.MasterOutputTrigger = TIM_TRGO_RESET;
    config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    hal_ret = HAL_TIMEx_MasterConfigSynchronization(htim, &config);
    configASSERT(hal_ret == HAL_OK);
}

// ----------------------------------------------------------------------------
// Overwritten HAL functions

/**
  * @brief This function handles TIM2 global interrupt (used for
  * Lid Stepper control)
  */
void TIM2_IRQHandler(void) { 
    HAL_TIM_IRQHandler(&_motor_hardware.lid_stepper.timer); 
}

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_DAC_IRQHandler(void) {
    HAL_TIM_IRQHandler(&_motor_hardware.seal.timer);
}

/**
 * @brief Callback for the Diag0 input - falling edge.
 * This flags a seal stepper error event
 */
void EXTI3_IRQHandler(void) {
	if(__HAL_GPIO_EXTI_GET_IT(SEAL_STEPPER_DIAG0_PIN) != 0x00u) {
		__HAL_GPIO_EXTI_CLEAR_IT(SEAL_STEPPER_DIAG0_PIN);
		if(_motor_hardware.callbacks.seal_stepper_error) {
			_motor_hardware.callbacks.seal_stepper_error(MOTOR_ERROR);
		}	
	}
}

/**
 * @brief Callback for the Diag1 input - falling edge.
 * This flags a seal stepper stall event.
 */
void EXTI4_IRQHandler(void) {
	if(__HAL_GPIO_EXTI_GET_IT(SEAL_STEPPER_DIAG1_PIN) != 0x00u) {
		__HAL_GPIO_EXTI_CLEAR_IT(SEAL_STEPPER_DIAG1_PIN);
		if(_motor_hardware.callbacks.seal_stepper_error) {
			_motor_hardware.callbacks.seal_stepper_error(MOTOR_STALL);
		}
	}
}

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
