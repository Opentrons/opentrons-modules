#include "stm32g4xx_hal.h"

/******************* Motor Z *******************/

/** Motor hardware **/
#define Z_STEP_PIN (GPIO_PIN_2)
#define Z_STEP_PORT (GPIOC)
#define Z_DIR_PIN (GPIO_PIN_1)
#define Z_DIR_PORT (GPIOC)
#define Z_EN_PIN (GPIO_PIN_3)
#define Z_EN_PORT (GPIOA)
#define Z_N_BRAKE_PIN(GPIO_PIN_7)
#define Z_N_BRAKE_PORT(GPIOB)

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
#define X_N_BRAKE_PIN(GPIO_PIN_9)
#define X_N_BRAKE_PORT(GPIOB)


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
#define L_EN_PIN (GPIO_PIN_4)
#define L_EN_PORT (GPIOA)


/** Limit switches **/
/* Note: Mechanical limit switches */
#define L_N_HELD_PIN (GPIO_PIN_5)
#define L_N_HELD_PORT (GPIOB)
#define L_N_RELEASED_PIN (GPIO_PIN_11)
#define L_N_RELEASED_PORT (GPIO_PIN_C)
