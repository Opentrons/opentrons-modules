#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
#include "mc_config.h"
#include "mc_interface.h"
#include "mc_tuning.h"
#include "stm32f3xx_hal.h"

typedef struct {
    bool open;
    bool closed;
} optical_switch_results;

typedef struct {
    ADC_HandleTypeDef adc1;
    ADC_HandleTypeDef adc2;
    TIM_HandleTypeDef tim1;
    TIM_HandleTypeDef tim2;
    TIM_HandleTypeDef tim3;
    DAC_HandleTypeDef dac1;
    MCI_Handle_t* mci[NBR_OF_MOTORS];
    MCT_Handle_t* mct[NBR_OF_MOTORS];
    void (*plate_lock_complete)(const optical_switch_results* results);
} motor_hardware_handles;

void motor_hardware_setup(motor_hardware_handles* handles);

void motor_hardware_solenoid_drive(DAC_HandleTypeDef* dac1, uint8_t dacval);
void motor_hardware_solenoid_release(DAC_HandleTypeDef* dac1);

void motor_hardware_plate_lock_on(TIM_HandleTypeDef* tim3, float power);
void motor_hardware_plate_lock_off(TIM_HandleTypeDef* tim3);
void motor_hardware_plate_lock_brake(TIM_HandleTypeDef* tim3);
bool motor_hardware_plate_lock_sensor_read(uint16_t GPIO_Pin);

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim);

/**
 * @brief To be called every time the motor control library updates its speed
 * measurement. This function filters the new speed value through an alpha
 * filter to reduce the noise in the data.
 * 
 * @param speed The new speed measurement to add to the average
 */
void motor_hardware_add_rpm_measurement(int16_t speed);

int16_t motor_hardware_get_smoothed_rpm();

// When filtering the RPM values, use this alpha constant
#define RPM_SPEED_FILTER_ALPHA (0.8)

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

#define PLATE_LOCK_TIM TIM3
#define PLATE_LOCK_Port GPIOE
#define PLATE_LOCK_NSLEEP_Pin GPIO_PIN_5
#define PLATE_LOCK_IN_1_Pin GPIO_PIN_2
#define PLATE_LOCK_IN_1_Chan TIM_CHANNEL_1
#define PLATE_LOCK_IN_2_Pin GPIO_PIN_3
#define PLATE_LOCK_IN_2_Chan TIM_CHANNEL_2
#define PLATE_LOCK_NFAULT_Pin GPIO_PIN_6
#define PLATE_LOCK_ENGAGED_Pin GPIO_PIN_0
#define PLATE_LOCK_RELEASED_Pin GPIO_PIN_4

// These defines drive the math for setting the PWM clocking parameters.
// The frequency will be respected as accurately as possible, and is in Hz.
// Because we only have ints available, the requested granularity will be less
// than or equal to whatever granularity we end up with - for instance, with
// 15535 (uint16_t max) requested, the prescaler needs to be 4.6; we'll set it
// to 4, and then the granularity will be 18000.
#define PLATE_LOCK_PWM_GRANULARITY_REQUESTED 15535uL
#define PLATE_LOCK_PWM_FREQ 1000uL
#define PLATE_LOCK_TIM_CLKDIV 1uL
#define PLATE_LOCK_INPUT_FREQ (72000000uL / PLATE_LOCK_TIM_CLKDIV)
#define PLATE_LOCK_TIM_PRESCALER \
    ((PLATE_LOCK_INPUT_FREQ) /   \
     (PLATE_LOCK_PWM_FREQ * PLATE_LOCK_PWM_GRANULARITY_REQUESTED))
#define PLATE_LOCK_PWM_GRANULARITY \
    ((PLATE_LOCK_INPUT_FREQ / PLATE_LOCK_TIM_PRESCALER) / PLATE_LOCK_PWM_FREQ)

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
