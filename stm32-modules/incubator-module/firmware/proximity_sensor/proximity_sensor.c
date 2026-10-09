#include "firmware/proximity_sensor.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "stm32g4xx_hal.h"
#include "task.h"

#define PROXIMITY_PC2_PIN GPIO_PIN_2
#define PROXIMITY_PC2_GPIO_PORT GPIOC
#define PROXIMITY_EVENT_QUEUE_LENGTH 8U
/* Numerically above the FreeRTOS syscall ceiling so the ISR may notify. */
#define PROXIMITY_EXTI_NVIC_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1)

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    IRQn_Type irq;
} proximity_pin_t;

static const proximity_pin_t pins[PROXIMITY_SENSOR_COUNT] = {
    {PROXIMITY_PC2_GPIO_PORT, PROXIMITY_PC2_PIN, EXTI2_IRQn},
};

static bool object_detected[PROXIMITY_SENSOR_COUNT];
static bool have_sample[PROXIMITY_SENSOR_COUNT];
static bool gpio_ready = false;
static bool irq_enabled = false;
static TaskHandle_t notify_task = NULL;
static QueueHandle_t event_queue = NULL;
static StaticQueue_t event_queue_state;
static proximity_event_t event_queue_storage[PROXIMITY_EVENT_QUEUE_LENGTH];

static bool pin_is_detected(proximity_sensor_id_t id) {
    return HAL_GPIO_ReadPin(pins[id].port, pins[id].pin) == GPIO_PIN_RESET;
}

static void notify_task_from_isr(void) {
    if (notify_task == NULL) {
        return;
    }
    BaseType_t higher_priority_woken = pdFALSE;
    vTaskNotifyGiveFromISR(notify_task, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

static void commit_state(proximity_sensor_id_t id, bool detected) {
    taskENTER_CRITICAL();
    object_detected[id] = detected;
    have_sample[id] = true;
    taskEXIT_CRITICAL();

    if (event_queue == NULL) {
        return;
    }
    /* Never block the sampler: a slow consumer loses events, not state. */
    proximity_event_t event = {.id = id, .object_detected = detected};
    (void)xQueueSend(event_queue, &event, 0);
}

static void sample_sensor(proximity_sensor_id_t id) {
    const bool raw = pin_is_detected(id);
    if (!have_sample[id] || raw != object_detected[id]) {
        commit_state(id, raw);
    }
}

void proximity_sensor_init(void) {
    if (gpio_ready) {
        return;
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();

    for (uint8_t id = 0; id < PROXIMITY_SENSOR_COUNT; ++id) {
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin = pins[id].pin;
        gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
        gpio.Pull = GPIO_PULLUP;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(pins[id].port, &gpio);
    }

    event_queue = xQueueCreateStatic(
        PROXIMITY_EVENT_QUEUE_LENGTH, sizeof(proximity_event_t),
        (uint8_t*)event_queue_storage, &event_queue_state);

    for (uint8_t id = 0; id < PROXIMITY_SENSOR_COUNT; ++id) {
        HAL_NVIC_SetPriority(pins[id].irq, PROXIMITY_EXTI_NVIC_PRIORITY, 0);
        HAL_NVIC_DisableIRQ(pins[id].irq);
    }
    gpio_ready = true;
}

void proximity_sensor_attach_task(TaskHandle_t task) {
    notify_task = task;
    if (irq_enabled) {
        proximity_sensor_poll();
        return;
    }

    for (uint8_t id = 0; id < PROXIMITY_SENSOR_COUNT; ++id) {
        __HAL_GPIO_EXTI_CLEAR_IT(pins[id].pin);
        NVIC_ClearPendingIRQ(pins[id].irq);
    }
    proximity_sensor_poll();
    for (uint8_t id = 0; id < PROXIMITY_SENSOR_COUNT; ++id) {
        HAL_NVIC_EnableIRQ(pins[id].irq);
    }
    irq_enabled = true;
}

void proximity_sensor_poll(void) {
    for (uint8_t id = 0; id < PROXIMITY_SENSOR_COUNT; ++id) {
        sample_sensor((proximity_sensor_id_t)id);
    }
}

bool proximity_sensor_read_raw(proximity_sensor_id_t id) {
    if (id >= PROXIMITY_SENSOR_COUNT) {
        return false;
    }
    return pin_is_detected(id);
}

bool proximity_sensor_read_registers(proximity_sensor_id_t id,
                                     proximity_pin_registers_t* out) {
    if (id >= PROXIMITY_SENSOR_COUNT || out == NULL) {
        return false;
    }
    const uint32_t pin = pins[id].pin;
    const uint32_t position = (uint32_t)__builtin_ctz(pin);
    GPIO_TypeDef* port = pins[id].port;
    out->idr = (port->IDR & pin) != 0U ? 1U : 0U;
    out->moder = (uint8_t)((port->MODER >> (position * 2U)) & 0x3U);
    out->pupdr = (uint8_t)((port->PUPDR >> (position * 2U)) & 0x3U);
    out->exti_imr = (EXTI->IMR1 & pin) != 0U ? 1U : 0U;
    return true;
}

bool proximity_sensor_object_detected(proximity_sensor_id_t id) {
    if (id >= PROXIMITY_SENSOR_COUNT) {
        return false;
    }
    taskENTER_CRITICAL();
    const bool detected = have_sample[id] && object_detected[id];
    taskEXIT_CRITICAL();
    return detected;
}

bool proximity_sensor_wait_event(proximity_event_t* event,
                                 TickType_t ticks_to_wait) {
    if (event == NULL || event_queue == NULL) {
        return false;
    }
    return xQueueReceive(event_queue, event, ticks_to_wait) == pdTRUE;
}

void EXTI2_IRQHandler(void) {
    if (__HAL_GPIO_EXTI_GET_IT(PROXIMITY_PC2_PIN) != 0U) {
        __HAL_GPIO_EXTI_CLEAR_IT(PROXIMITY_PC2_PIN);
        notify_task_from_isr();
    }
}
