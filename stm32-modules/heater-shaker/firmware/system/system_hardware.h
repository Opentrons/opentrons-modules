#ifndef SYSTEM_HARDWARE_H__
#define SYSTEM_HARDWARE_H__
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>

typedef struct {
    bool success;
    bool error;
} led_transmit_result;

typedef struct {
    void (*led_transmit_complete)(const led_transmit_result* result);
    void (*led_transmit_error_complete)(const led_transmit_result* result);
} system_hardware_handles;

typedef enum {
    PWM_Init,
    LED_Control
} I2C_Operations;

#define I2C_ADDRESS        0x6C
/* I2C TIMING Register define when I2C clock source is SYSCLK */
/* I2C TIMING is calculated in case of the I2C Clock source is the SYSCLK = 72 MHz */
/* This example use TIMING to 0x00C4092A to reach 1 MHz speed (Rise time = 26ns, Fall time = 2ns) */
#define I2C_TIMING      0x00201D2B

/* Definition for I2Cx clock resources */
#define I2Cx                            I2C1
#define RCC_PERIPHCLK_I2Cx              RCC_PERIPHCLK_I2C1
#define RCC_I2CxCLKSOURCE_SYSCLK        RCC_I2C1CLKSOURCE_SYSCLK
#define I2Cx_CLK_ENABLE()               __HAL_RCC_I2C1_CLK_ENABLE()
#define I2Cx_SDA_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE()
#define I2Cx_SCL_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOB_CLK_ENABLE() 

#define I2Cx_FORCE_RESET()              __HAL_RCC_I2C1_FORCE_RESET()
#define I2Cx_RELEASE_RESET()            __HAL_RCC_I2C1_RELEASE_RESET()

/* Definition for I2Cx Pins */
#define I2Cx_SCL_PIN                    GPIO_PIN_6
#define I2Cx_SCL_GPIO_PORT              GPIOB
#define I2Cx_SDA_PIN                    GPIO_PIN_7
#define I2Cx_SDA_GPIO_PORT              GPIOB
#define I2Cx_SCL_SDA_AF                 GPIO_AF4_I2C1

/* Definition for I2Cx's NVIC */
#define I2Cx_EV_IRQn                    I2C1_EV_IRQn
#define I2Cx_ER_IRQn                    I2C1_ER_IRQn
#define I2Cx_EV_IRQHandler              I2C1_EV_IRQHandler
#define I2Cx_ER_IRQHandler              I2C1_ER_IRQHandler

#define BASE_PWM_REGISTER               0x04
#define UPDATE_REGISTER                 0x13
#define BASE_REGISTER                   0x17 //first LED is on driver channel 4
#define REGISTER_SIZE                   0x01

#define LED_OUTPUT_HIGH                 0x30
#define LED_PWM_OUT_HI                  0xFF

#define SOFTPOWER_BUTTON_SENSE_PIN GPIO_PIN_4
#define SOFTPOWER_UNPLUG_SENSE_PIN GPIO_PIN_5
#define SOFTPOWER_PORT GPIOB

void system_hardware_setup(system_hardware_handles* handles);
void system_hardware_enter_bootloader(void);
bool system_hardware_set_led_original(uint8_t* aTxBuffer, I2C_Operations operation);
bool system_hardware_set_led(uint8_t step, uint8_t which);
bool system_hardware_I2C_ready(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  // _SYSTEM_HARDWARE_H__
