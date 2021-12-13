#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_tim.h"
#include <stdbool.h>

typedef struct {
    int32_t step_count;
    int32_t step_target;
} motor_hardware_status;

typedef struct {
    //update
    //TIM_HandleTypeDef tim2;
    //TIM_OC_InitTypeDef tim2_oc;
    DAC_HandleTypeDef dac1;
    motor_hardware_status lid_stepper;
    void (*lid_stepper_complete)();
} motor_hardware_handles;

extern TIM_HandleTypeDef htim2;

void motor_hardware_setup(motor_hardware_handles* handles);

void motor_hardware_lid_stepper_start(float angle);
void motor_hardware_lid_stepper_stop();
void motor_hardware_increment_step();
void motor_hardware_lid_stepper_set_dac(uint8_t dacval);
bool motor_hardware_lid_stepper_reset(void);
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

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
