#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/**
 * Panasonic GX-F6A inductive proximity sensors on the Nucleo-G491RE.
 *
 * The GX-F6A is NPN normally-open: the black wire sinks to GND while metal
 * is in range and is otherwise open. The board has an external pull-up on
 * PC2, so the GPIO is configured with no internal pull. A low pin means an
 * object is detected.
 *
 * Connect the black wire to the GPIO. The external pull-up holds the line at
 * 3.3 V while the output is open. Enabling the internal pull-up as well
 * holds the pin high against the sensor. The sensor supply is 12-24 V; do
 * not pull the output up to that rail, or the STM32 pin will be damaged.
 * Nucleo morpho CN7 pin 35 is PC2. PC3 (CN7 pin 37) is not in use.
 */

/**
 * How often the background task re-reads the pins.
 * The GX-F6A response frequency is 400 Hz (2.5 ms). The FreeRTOS tick is
 * 1 ms, which is the shortest interval this loop can use.
 */
#define PROXIMITY_SAMPLE_MS 1U

typedef enum {
    PROXIMITY_SENSOR_PC2 = 0,
    PROXIMITY_SENSOR_COUNT
} proximity_sensor_id_t;

typedef struct {
    proximity_sensor_id_t id;
    bool object_detected;
} proximity_event_t;

/**
 * @brief Configure PC2 as an input with no pull and create the event queue.
 *
 * EXTI stays masked until proximity_sensor_attach_task(). Call this before
 * the scheduler starts.
 */
void proximity_sensor_init(void);

/**
 * @brief Bind the proximity task, publish the current levels, then enable EXTI.
 * @param task Task notified from EXTI2 and EXTI3. Must be the caller.
 */
void proximity_sensor_attach_task(TaskHandle_t task);

/**
 * @brief Sample both pins once, debounce, and queue any accepted change.
 *
 * Call every PROXIMITY_SAMPLE_MS and after each EXTI notification, only from
 * the task passed to proximity_sensor_attach_task().
 */
void proximity_sensor_poll(void);

/**
 * @brief Undebounced pin level.
 * @param id Sensor channel.
 * @return true when the pin currently reads low (output sinking).
 */
bool proximity_sensor_read_raw(proximity_sensor_id_t id);

typedef struct {
    uint8_t idr;      /**< Input level: 1 = high (open), 0 = low (sinking). */
    uint8_t moder;    /**< 0 = input, 1 = output, 2 = alternate, 3 = analog. */
    uint8_t pupdr;    /**< 0 = none, 1 = pull-up, 2 = pull-down. */
    uint8_t exti_imr; /**< 1 when the EXTI line is unmasked. */
} proximity_pin_registers_t;

/**
 * @brief Snapshot of the GPIO and EXTI configuration for one sensor pin.
 * @return false when @p id or @p out is invalid.
 */
bool proximity_sensor_read_registers(proximity_sensor_id_t id,
                                     proximity_pin_registers_t* out);

/**
 * @brief Latest debounced detection state.
 * @param id Sensor channel.
 * @return true when that GX-F6A output is sinking.
 */
bool proximity_sensor_object_detected(proximity_sensor_id_t id);

/**
 * @brief Block until a sensor level changes.
 * @param event Receives the channel and the new detection state.
 * @param ticks_to_wait FreeRTOS tick timeout. Use portMAX_DELAY to wait.
 * @return true when an event was copied to @p event.
 */
bool proximity_sensor_wait_event(proximity_event_t* event,
                                 TickType_t ticks_to_wait);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
