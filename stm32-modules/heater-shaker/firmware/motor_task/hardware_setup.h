#ifndef __HARDWARE_SETUP_H
#define __HARDWARE_SETUP_H
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include "stm32f3xx_hal.h"

void hardware_setup();

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#define M1_CURR_AMPL_U_Pin GPIO_PIN_1
#define M1_CURR_AMPL_U_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define M1_CURR_AMPL_W_Pin GPIO_PIN_7
#define M1_CURR_AMPL_W_GPIO_Port GPIOA
#define M1_TEMPERATURE_Pin GPIO_PIN_4
#define M1_TEMPERATURE_GPIO_Port GPIOC
#define M1_BUS_VOLTAGE_Pin GPIO_PIN_5
#define M1_BUS_VOLTAGE_GPIO_Port GPIOC
#define M1_HALL_H3_Pin GPIO_PIN_10
#define M1_HALL_H3_GPIO_Port GPIOB
#define M1_CURR_AMPL_V_Pin GPIO_PIN_11
#define M1_CURR_AMPL_V_GPIO_Port GPIOB
#define M1_PWM_EN_U_Pin GPIO_PIN_13
#define M1_PWM_EN_U_GPIO_Port GPIOB
#define M1_PWM_EN_V_Pin GPIO_PIN_14
#define M1_PWM_EN_V_GPIO_Port GPIOB
#define M1_PWM_EN_W_Pin GPIO_PIN_15
#define M1_PWM_EN_W_GPIO_Port GPIOB
#define M1_PWM_UH_Pin GPIO_PIN_8
#define M1_PWM_UH_GPIO_Port GPIOA
#define M1_PWM_VH_Pin GPIO_PIN_9
#define M1_PWM_VH_GPIO_Port GPIOA
#define M1_PWM_WH_Pin GPIO_PIN_10
#define M1_PWM_WH_GPIO_Port GPIOA
#define M1_OCP_Pin GPIO_PIN_11
#define M1_OCP_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define M1_HALL_H1_Pin GPIO_PIN_15
#define M1_HALL_H1_GPIO_Port GPIOA
#define M1_HALL_H2_Pin GPIO_PIN_3
#define M1_HALL_H2_GPIO_Port GPIOB


#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
#endif // __HARDWARE_SETUP_H
