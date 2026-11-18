#include "firmware/system_hardware.h"

#include "main.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_cortex.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal_tim.h"

/** Local defines */
// This is the start of the sys memory region for the STM32G491
// from the reference manual and STM application note AN2606
#define SYSMEM_START 0x1fff0000
#define SYSMEM_BOOT (SYSMEM_START + 4)

// address 4 in the bootable region is the address of the first instruction that
// should run, aka the data that should be loaded into $pc.
const uint32_t *const sysmem_boot_loc = (uint32_t *)SYSMEM_BOOT;
uint16_t reset_reason;

enum RCC_FLAGS {
    // lse clock security system failure
    LSECSSD,  // = 0
    // brown out
    BORRST,  // = 1
    // option byte-loader reset
    OBLRST,  // = 2
    // pin reset
    PINRST,  // = 3
    // software reset
    SFTRST,  // = 4
    // independent watchdog
    IWDGRST,  // = 5
    // window watchdog
    WWDGRST,  // = 6
    // low power reset
    LPWRRST,  // = 7
};

static void save_reset_reason() {
    // check various reset flags to see if the HAL RCC
    // reset flag matches any of them
    reset_reason = 0;

    // lse clock security system failure
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSECSSD)) {
        reset_reason |= (1 << LSECSSD);
    }
    // brown out
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST)) {
        reset_reason |= (1 << BORRST);
    }
    // option byte-loader reset
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST)) {
        reset_reason |= (1 << OBLRST);
    }
    // pin reset
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
        reset_reason |= (1 << PINRST);
    }
    // software reset
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
        reset_reason |= (1 << SFTRST);
    }
    // independent watchdog
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        reset_reason |= (1 << IWDGRST);
    }
    // window watchdog
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) {
        reset_reason |= (1 << WWDGRST);
    }
    // low power reset
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)) {
        reset_reason |= (1 << LPWRRST);
    }
}

uint16_t system_hardware_reset_reason() { return reset_reason; }

/** PUBLIC FUNCTION IMPLEMENTATION */

void system_hardware_enter_bootloader(void) {
    // We have to uninitialize as many of the peripherals as possible, because
    // the bootloader expects to start as the system comes up

    // The HAL has ways to turn off all the core clocking and the clock security
    // system
    HAL_RCC_DisableLSECSS();
    HAL_RCC_DeInit();

    // systick should be off at boot
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    /* Clear Interrupt Enable Register & Interrupt Pending Register */
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    // We have to make sure that the processor is mapping the system memory
    // region to address 0, which the bootloader expects
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
    // and now we're ready to set the system up to start executing system flash.
    // arm cortex initialization means that
    // address 0 in the bootable region is the address where the processor
    // should start its stack which we have to do as late as possible because as
    // soon as we do this the c and c++ runtime environment is no longer valid
    __set_MSP(*((uint32_t *)SYSMEM_START));

    // finally, jump to the bootloader. we do this in inline asm because we need
    // this to be a naked call (no caller-side prep like stacking return
    // addresses) and to have a naked function you need to define it as a
    // function, not a function pointer, and we don't statically know the
    // address here since it is whatever's contained in that second word of the
    // bsystem memory region.
    asm volatile("bx %0"
                 :  // no outputs
                 : "r"(*sysmem_boot_loc)
                 : "memory");
}

/**
 * @brief enable the eeprom write protect pin.
 */
void eeprom_write_protect_init(void) {
    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pin : PA10 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = EEPROM_WP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(EEPROM_WP_PORT, &GPIO_InitStruct);
}

/**
 * enable/disable writing to the eeprom.
 */
void enable_eeprom_write(bool enable) {
    HAL_GPIO_WritePin(EEPROM_WP_PORT, EEPROM_WP_PIN,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void system_hardware_gpio_init(void) {
    save_reset_reason();
    eeprom_write_protect_init();
    // Disable eeprom write
    enable_eeprom_write(false);
}
