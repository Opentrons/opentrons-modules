#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
#include "mc_interface.h"
#include "mc_tuning.h"
#include "stm32f3xx_hal.h"

void motor_hardware_setup(ADC_HandleTypeDef* adc1, ADC_HandleTypeDef* adc2,
                          TIM_HandleTypeDef* tim1, TIM_HandleTypeDef* tim2,
                          MCI_Handle_t* mci[], MCT_Handle_t* mct[],
                          DAC_HandleTypeDef* dac1);

void motor_hardware_solenoid_drive(DAC_HandleTypeDef* dac1, uint8_t dacval);
void motor_hardware_solenoid_release(DAC_HandleTypeDef* dac1);

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim);

#define MC_HAL_IS_USED

// Drive and current sense pins
#define M1_CURR_AMPL_U_Pin GPIO_PIN_1
#define M1_CURR_AMPL_U_GPIO_Port GPIOA
#define M1_CURR_AMPL_W_Pin GPIO_PIN_7
#define M1_CURR_AMPL_W_GPIO_Port GPIOA
#define M1_PWM_UH_Pin GPIO_PIN_8
#define M1_PWM_UH_GPIO_Port GPIOA
#define M1_PWM_VH_Pin GPIO_PIN_9
#define M1_PWM_VH_GPIO_Port GPIOA
#define M1_PWM_WH_Pin GPIO_PIN_10
#define M1_PWM_WH_GPIO_Port GPIOA

#define M1_CURR_AMPL_V_Pin GPIO_PIN_11
#define M1_CURR_AMPL_V_GPIO_Port GPIOB
#define M1_PWM_EN_U_Pin GPIO_PIN_13
#define M1_PWM_EN_U_GPIO_Port GPIOB
#define M1_PWM_EN_V_Pin GPIO_PIN_14
#define M1_PWM_EN_V_GPIO_Port GPIOB
#define M1_PWM_EN_W_Pin GPIO_PIN_15
#define M1_PWM_EN_W_GPIO_Port GPIOB

/*
#define M1_TEMPERATURE_Pin GPIO_PIN_4
#define M1_TEMPERATURE_GPIO_Port GPIOC
*/

// Safety system pins. The OCP pin is driven low by the driver when an
// overcurrent event occurs; the bus voltage pin is tripped when motor
// bus voltage falls too far.
#define M1_OCP_Pin GPIO_PIN_3
#define M1_OCP_GPIO_Port GPIOC
#define M1_BUS_VOLTAGE_Pin GPIO_PIN_5
#define M1_BUS_VOLTAGE_GPIO_Port GPIOC

// Hall sensor pins that sense the mechanical phase angle of the rotor
#define M1_HALL_H1_Pin GPIO_PIN_3
#define M1_HALL_H1_GPIO_Port GPIOD
#define M1_HALL_H2_Pin GPIO_PIN_4
#define M1_HALL_H2_GPIO_Port GPIOD
#define M1_HALL_H3_Pin GPIO_PIN_7
#define M1_HALL_H3_GPIO_Port GPIOD

#define DRIVER_NSLEEP_Port GPIOC
#define DRIVER_NSLEEP_Pin GPIO_PIN_2

// The homing solenoid is driven by an integrated package h-bridge controller
// Allegro A4950KLJTR-T. It has two input pins that in theory control both
// direction and power; here, really only one direction matters. Pin 2 is
// held low at all times; pin 1 is driven high to send energy (with the amount
// controlled by a DAC setting the current limit to the driver rather than
// PWMing) to move the solenoid, and driven low to put the driver into coast
// mode and let the solenoid's spring retract it.
#define SOLENOID_1_Port GPIOC
#define SOLENOID_1_Pin GPIO_PIN_6
#define SOLENOID_2_Port GPIOC
#define SOLENOID_2_Pin GPIO_PIN_7
// Pin A5 / DAC1_OUT2 is the DAC output channel that controls the reference
// voltage for the solenoid current limiter
#define SOLENOID_VREF_Port GPIOA
#define SOLENOID_VREF_Pin GPIO_PIN_5
#define SOLENOID_DAC_CHANNEL DAC_CHANNEL_2

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
