#include "firmware/system_hardware.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal_cortex.h"
#include "stm32g4xx_hal_tim.h"

/** Private definitions.*/
#define DBG_LED_PIN GPIO_PIN_6
#define DBG_LED_PORT GPIOE

#define BUTTON_LED_PIN (GPIO_PIN_10)
#define BUTTON_LED_PORT (GPIOD)

#define FRONT_BUTTON_IN_PIN (GPIO_PIN_13)
#define FRONT_BUTTON_IN_PORT (GPIOC)
#define FRONT_BUTTON_IRQ (EXTI15_10_IRQn)

#define FRONT_BUTTON_IN_PIN_REV1 (GPIO_PIN_11)
#define FRONT_BUTTON_IN_PORT_REV1 (GPIOD)

/** Global variable instantiation */

typedef struct SystemHardware_struct {
    uint32_t button_last_tick;
    front_button_callback_t button_callback;

    // Port changes based on hardware rev
    GPIO_TypeDef *front_button_in_port;
    // Pin changes basd on hardware rev
    uint16_t front_button_in_pin;
} SystemHardware_t;

static SystemHardware_t _system = {
    .button_callback = 0,
    .button_callback = NULL,
    .front_button_in_port = FRONT_BUTTON_IN_PORT,
    .front_button_in_pin = FRONT_BUTTON_IN_PIN
};

/** PUBLIC FUNCTION IMPLEMENTATION */

/**
 * Initialize hardware specific to the system process:
 * - PE6 = Heartbeat LED
 * - PD10 = Front switch LED
 * - PC13 = Front switch input
 */
void system_hardware_setup(bool rev_1_board, front_button_callback_t button_cb) {
    GPIO_InitTypeDef gpio_init = {
      .Pin = DBG_LED_PIN,
      .Mode = GPIO_MODE_OUTPUT_PP,
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_LOW
    };
    __HAL_RCC_GPIOE_CLK_ENABLE();
    HAL_GPIO_Init(DBG_LED_PORT, &gpio_init);

    gpio_init.Pin = BUTTON_LED_PIN;
    __HAL_RCC_GPIOD_CLK_ENABLE();
    HAL_GPIO_Init(BUTTON_LED_PORT, &gpio_init);
    // Initialize the LED pin on to turn it on
    HAL_GPIO_WritePin(BUTTON_LED_PORT, BUTTON_LED_PIN, GPIO_PIN_SET);

    if(rev_1_board) {
        _system.front_button_in_port = FRONT_BUTTON_IN_PORT_REV1;
        _system.front_button_in_pin = FRONT_BUTTON_IN_PIN_REV1;
    }

    gpio_init.Pin = _system.front_button_in_pin;
    gpio_init.Mode = GPIO_MODE_IT_FALLING;
    __HAL_RCC_GPIOC_CLK_ENABLE();
    HAL_GPIO_Init(_system.front_button_in_port, &gpio_init);

    HAL_NVIC_SetPriority(FRONT_BUTTON_IRQ, 5, 0);
    HAL_NVIC_EnableIRQ(FRONT_BUTTON_IRQ);

    _system.button_last_tick = HAL_GetTick();
    _system.button_callback = button_cb;
}

// This is the start of the sys memory region for the STM32G491 
// from the reference manual and STM application note AN2606
#define SYSMEM_START 0x1fff0000
#define SYSMEM_BOOT (SYSMEM_START + 4)

// address 4 in the bootable region is the address of the first instruction that
// should run, aka the data that should be loaded into $pc.
const uint32_t *const sysmem_boot_loc = (uint32_t*)SYSMEM_BOOT;

void system_debug_led(int set)
{
    GPIO_PinState state = (set > 0) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_6, state);
}

void system_hardware_enter_bootloader(void) {

    // We have to uninitialize as many of the peripherals as possible, because the bootloader
    // expects to start as the system comes up

    // The HAL has ways to turn off all the core clocking and the clock security system
    HAL_RCC_DisableLSECSS();
    HAL_RCC_DeInit();

    // systick should be off at boot
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;


    /* Clear Interrupt Enable Register & Interrupt Pending Register */
    for (int i=0;i<8;i++)
    {
        NVIC->ICER[i]=0xFFFFFFFF;
        NVIC->ICPR[i]=0xFFFFFFFF;
    }

    // We have to make sure that the processor is mapping the system memory region to address 0,
    // which the bootloader expects
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
    // and now we're ready to set the system up to start executing system flash.
    // arm cortex initialization means that

    // address 0 in the bootable region is the address where the processor should start its stack
    // which we have to do as late as possible because as soon as we do this the c and c++ runtime
    // environment is no longer valid
    __set_MSP(*((uint32_t*)SYSMEM_START));

    // finally, jump to the bootloader. we do this in inline asm because we need
    // this to be a naked call (no caller-side prep like stacking return addresses)
    // and to have a naked function you need to define it as a function, not a
    // function pointer, and we don't statically know the address here since it is
    // whatever's contained in that second word of the bsystem memory region.
    asm volatile (
        "bx %0"
        : // no outputs
        : "r" (*sysmem_boot_loc)
        : "memory"  );
}

bool system_front_button_pressed(void) {
    // Active low button, passively pulled high
    return HAL_GPIO_ReadPin(_system.front_button_in_port, _system.front_button_in_pin) 
                == GPIO_PIN_RESET;
}

void system_front_button_led_set(bool set) {
    HAL_GPIO_WritePin(BUTTON_LED_PORT, BUTTON_LED_PIN,
        set ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void system_front_button_callback(void) {
    if(__HAL_GPIO_EXTI_GET_IT(_system.front_button_in_pin) != 0x00u) {
        __HAL_GPIO_EXTI_CLEAR_IT(_system.front_button_in_pin);
        uint32_t new_tick = HAL_GetTick();
        if((new_tick - _system.button_last_tick) > 
            FRONT_BUTTON_DEBOUNCE_MS) {

            _system.button_last_tick = new_tick;
            if(_system.button_callback) {
                _system.button_callback();
            }
        }
    }
}

void system_hardware_jump_from_exception(void) {
    // Okay, so we're in an exception (hard fault, bus fault, etc) and want to
    // jump to the DFU bootloader. We are going to jump to the function for
    // jumping to the bootloader, but to get there we have to exit this 
    // exception context. In order to do this, we have to do a few things:
    //   1. Clear the CFSR and HFSR status registers, or the bootloader will
    //      refuse to run.
    //   2. Update the PC in the exception stack frame. This step means that
    //      we HAVE to run only naked function calls, which means nothing but
    //      assembly code is allowed.
    //   3. Update the execution mode of the PSR in the exception stack frame.
    //      If this is an invalid value, the processor will be locked forever,
    //      so we force it to 0x10 for User Mode.
    //   3. Ovewrite the link register with a known exception pattern, and
    //      then return to our overwritten PC value by bx'ing to it
    asm volatile (
        /* Clear CFSR register.*/
        "ldr r0, =0xE000ED28\n"
        "ldr r1, [r0]\n"
        "str r1, [r0]\n"
        /* Clear HFSR register.*/
        "ldr r0, =0xE000ED2C\n"
        "ldr r1, [r0]\n"
        "str r1, [r0]\n"
        /* Update the PC in the stack frame.*/
        /* https://developer.arm.com/documentation/dui0552/a/the-cortex-m3-processor/exception-model/exception-entry-and-return */
        "ldr r0, bootloader_address_const\n"
        "str r0, [sp,#0x18]\n"
        /* In case the PSR is in invalid state, force to user mode*/
        "ldr r1, [sp,#0x1C]\n"
        "and r1, r1, #0xFFFFFFF0\n"
        "orr r1, r1, #0x10\n"
        "str r1, [sp,#0x1C]\n"
        /* Leave the exception handler */
        "ldr lr,=0xFFFFFFF1\n"
        "bx  lr\n"
        "bootloader_address_const: .word system_hardware_enter_bootloader"
        : // no outputs
        : // no inputs
        : "memory"  
    );
}
