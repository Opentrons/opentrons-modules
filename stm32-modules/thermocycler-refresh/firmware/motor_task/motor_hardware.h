#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
#include "stm32g4xx_hal.h"

typedef struct {
    //update
    //ADC_HandleTypeDef adc1;
    //ADC_HandleTypeDef adc2;
    //TIM_HandleTypeDef tim1;
    TIM_HandleTypeDef tim2;
    //TIM_HandleTypeDef tim3;
    DAC_HandleTypeDef dac1;
    //MCI_Handle_t* mci[NBR_OF_MOTORS];
    //MCT_Handle_t* mct[NBR_OF_MOTORS];
} motor_hardware_handles;

void motor_hardware_setup(motor_hardware_handles* handles);

void motor_hardware_solenoid_engage();
void motor_hardware_solenoid_release();

// The homing solenoid is driven by an integrated package h-bridge controller
// Allegro A4950KLJTR-T. It has two input pins that in theory control both
// direction and power; here, really only one direction matters. Pin 2 is
// held low at all times; pin 1 is driven high to send energy (with the amount
// controlled by a DAC setting the current limit to the driver rather than
// PWMing) to move the solenoid, and driven low to put the driver into coast
// mode and let the solenoid's spring retract it.

// fix comment
#define SOLENOID_Port GPIOD
#define SOLENOID_Pin GPIO_PIN_8

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
