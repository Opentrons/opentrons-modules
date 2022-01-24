#ifndef __MOTOR_HARDWARE_H
#define __MOTOR_HARDWARE_H
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

// ----------------------------------------------------------------------------
// Type definitions

// Void return and no parameters
typedef void (*lid_callback_t)(void);

// This structure is used to define callbacks out of motor interrupts
typedef struct {
    lid_callback_t lid_stepper_complete;
} motor_hardware_callbacks;

// ----------------------------------------------------------------------------
// Function definitions

void motor_hardware_setup(const motor_hardware_callbacks* callbacks);

void motor_hardware_lid_stepper_start(int32_t steps);
void motor_hardware_lid_stepper_stop();
void motor_hardware_increment_step();
void motor_hardware_lid_stepper_set_dac(uint8_t dacval);
bool motor_hardware_lid_stepper_check_fault(void);
bool motor_hardware_lid_stepper_reset(void);
void motor_hardware_solenoid_engage();
void motor_hardware_solenoid_release();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __MOTOR_HARDWARE_H
