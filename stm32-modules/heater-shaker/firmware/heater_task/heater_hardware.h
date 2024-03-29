#ifndef __HEATER_HARDWARE_H_
#define __HEATER_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stddef.h>

#include "stm32f3xx_hal.h"
#include "systemwide.h"

// These defines drive the math for setting the PWM clocking parameters.
// The frequency will be respected as accurately as possible, and is in Hz.
// Because we only have ints available, the requested granularity will be less
// than or equal to whatever granularity we end up with - for instance, with
// 15535 (uint16_t max) requested, the prescaler needs to be 4.6; we'll set it
// to 4, and then the granularity will be 18000.
#define HEATER_PAD_PWM_GRANULARITY_REQUESTED 15535uL
#define HEATER_PAD_PWM_FREQ 1000uL
#define HEATER_PAD_TIM_CLKDIV 1uL
#define HEATER_PAD_INPUT_FREQ (72000000uL / HEATER_PAD_TIM_CLKDIV)
#define HEATER_PAD_TIM_PRESCALER \
    ((HEATER_PAD_INPUT_FREQ) /   \
     (HEATER_PAD_PWM_FREQ * HEATER_PAD_PWM_GRANULARITY_REQUESTED))
#define HEATER_PAD_PWM_GRANULARITY \
    ((HEATER_PAD_INPUT_FREQ / HEATER_PAD_TIM_PRESCALER) / HEATER_PAD_PWM_FREQ)
#define HEATER_PAD_OPEN_CHECK_PULSE_DIVIDER 10uL
#define HEATER_PAD_OPEN_CHECK_PULSE \
    (HEATER_PAD_PWM_GRANULARITY /   \
     HEATER_PAD_OPEN_CHECK_PULSE_DIVIDER)  // 10% of pwm period
#define HEATER_PAD_OPEN_CHECK_THRESHOLD_FACTOR 2uL
#define HEATER_PAD_OPEN_CHECK_THRESHOLD \
    (HEATER_PAD_OPEN_CHECK_PULSE *      \
     HEATER_PAD_OPEN_CHECK_THRESHOLD_FACTOR)  // 20% of pwm period
#define HEATER_PAD_SHORT_CHECK_THRESHOLD_FACTOR 8uL
#define HEATER_PAD_SHORT_CHECK_PULSE_FACTOR 9uL
#define HEATER_PAD_SHORT_CHECK_THRESHOLD \
    (HEATER_PAD_OPEN_CHECK_PULSE *       \
     HEATER_PAD_SHORT_CHECK_THRESHOLD_FACTOR)  // 80% of pwm period
#define HEATER_PAD_SHORT_CHECK_PULSE \
    (HEATER_PAD_OPEN_CHECK_PULSE *   \
     HEATER_PAD_SHORT_CHECK_PULSE_FACTOR)                 // 90% of pwm period
#define HEATER_PAD_CIRCUIT_DAC_THRESHOLD 0x00000F8U       // roughly 0.2V
#define HEATER_PAD_OVERCURRENT_DAC_THRESHOLD 0x00000F82U  // roughly 3.2V
#define HEATER_PAD_CIRCUIT_CHECK_PERIOD \
    2000uL  // count incremented twice per tick, so period is half this value
            // (in ticks)

typedef enum {
    NTC_PAD_A = 1,
    NTC_PAD_B = 2,
    NTC_ONBOARD = 3,
} ntc_selection;

typedef struct {
    uint16_t pad_a_val;
    uint16_t pad_b_val;
    uint16_t onboard_val;
} conversion_results;

typedef enum {
    IDLE = 1,
    RUNNING = 2,
    PREP_CHECK = 3,
    OPEN_CHECK_STARTED = 4,
    OPEN_CHECK_COMPLETE = 5,
    SHORT_CHECK_STARTED = 6,
    SHORT_CHECK_COMPLETE = 7,
    PREP_RUNNING = 8,
    ERROR_OPEN_CIRCUIT = 9,
    ERROR_SHORT_CIRCUIT = 10,
    ERROR_OVERCURRENT = 11,
} heatpad_cs_state;

struct __attribute__((packed)) writable_offsets {
    uint64_t const_b;
    uint64_t const_c;
    uint64_t const_flag;
};

typedef struct {
    void (*conversions_complete)(const conversion_results* results);
    void* hardware_internal;
} heater_hardware;

void heater_hardware_setup(heater_hardware* hardware);
void heater_hardware_teardown(heater_hardware* hardware);
void heater_hardware_begin_conversions(heater_hardware* hardware);
bool heater_hardware_sense_power_good();
void heater_hardware_drive_pg_latch_low();
void heater_hardware_release_pg_latch();
void heater_hardware_power_disable(heater_hardware* hardware);
HEATPAD_CIRCUIT_ERROR heater_hardware_power_set(heater_hardware* hardware,
                                                uint16_t setting);
bool heater_hardware_set_offsets(struct writable_offsets* to_write);
uint64_t heater_hardware_get_offset(size_t addr_offset);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // __HEATER_HARDWARE_H_
