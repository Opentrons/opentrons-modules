#include "firmware/thermistor_hardware.h"

#include <stdbool.h>
#include <stdatomic.h>

// HAL includes
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_it.h"
#include "stm32g4xx_hal_i2c.h"

// FreeRTOS includes
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"


/** Private definitions */

#define ADC_ALERT_PIN  (GPIO_PIN_12)
#define ADC_ALERT_PORT (GPIOC)

/** Private typedef */

struct ThermistorHardware {
    // Used to signal the ADC Alert pin interrupt
    _Atomic TaskHandle_t gpio_task_to_notify;
    _Atomic bool initialized;
    _Atomic bool initialization_started;
};

/** Static variables */

static struct ThermistorHardware hardware = {
    .gpio_task_to_notify = NULL,
    .initialized = false,
    .initialization_started = false,
};

/** Public functions */

void thermistor_hardware_init() {
    GPIO_InitTypeDef gpio_init = {0};

    // Enforce that only one task may initialize the I2C
    if(atomic_exchange(&hardware.initialization_started, true) == false) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* Configure the ADC Alert pin*/
        gpio_init.Pin = ADC_ALERT_PIN;
        gpio_init.Mode = GPIO_MODE_IT_FALLING;
        gpio_init.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(ADC_ALERT_PORT, &gpio_init);

        /** Configure interrupt for ADC Alert pin */
        HAL_NVIC_SetPriority(EXTI15_10_IRQn, 4, 0);
        HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

        hardware.initialized = true;
    } else {
        // Spin until the hardware is initialized
        while(!hardware.initialized) {
            taskYIELD();
        }
    }
}


bool thermal_arm_adc_for_read() {
    hardware.gpio_task_to_notify = xTaskGetCurrentTaskHandle();
    return true;
}

void thermal_adc_ready_callback() {
    // Check that the pin is actually set - the interrupt doesn't do this for us,
    // and other pins trigger the same interrupt vector.
    if(__HAL_GPIO_EXTI_GET_IT(ADC_ALERT_PIN) != 0x00u) {
        __HAL_GPIO_EXTI_CLEAR_IT(ADC_ALERT_PIN);
        // There's a possibility of getting an interrupt when we don't expect
        // one, so just ignore if there's no armed task.
        if(hardware.gpio_task_to_notify != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            vTaskNotifyGiveFromISR( hardware.gpio_task_to_notify, &xHigherPriorityTaskWoken );
            hardware.gpio_task_to_notify = NULL;
            portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        }
    }
}
