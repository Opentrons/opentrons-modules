#include "firmware/system_hardware.h"

#include "main.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_cortex.h"
#include "stm32g4xx_hal_flash.h"
#include "stm32g4xx_hal_flash_ex.h"
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

/**
 * Program nSWBOOT0 / nBOOT0 and optionally launch (resets the MCU).
 *
 * @param system_memory_boot  true  -> boot system-memory DFU (nSWBOOT0=0,nBOOT0=0)
 *                            false -> normal boot from pin/flash (nSWBOOT0=1,nBOOT0=1)
 * @param launch              if true, HAL_FLASH_OB_Launch() (does not return on success)
 * @return true if the option-byte program step succeeded
 */
static bool program_boot_option_bytes(bool system_memory_boot, bool launch) {
    FLASH_OBProgramInitTypeDef ob_init = {0};

    // Clear sticky flash errors left by prior ops / bootloader (required or
    // OPTSTRT can fail and leave nSWBOOT0 stuck selecting system memory).
    __HAL_RCC_FLASH_CLK_ENABLE();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return false;
    }
    if (HAL_FLASH_OB_Unlock() != HAL_OK) {
        HAL_FLASH_Lock();
        return false;
    }

    ob_init.OptionType = OPTIONBYTE_USER;
    ob_init.USERType = OB_USER_nSWBOOT0 | OB_USER_nBOOT0;
    if (system_memory_boot) {
        // nSWBOOT0=0 (boot mode from option bit), nBOOT0=0 (system memory)
        ob_init.USERConfig = OB_BOOT0_FROM_OB | OB_nBOOT0_RESET;
    } else {
        // Match factory-ish default OPTR=0xFFEFF8AA: boot from BOOT0 pin,
        // nBOOT0 set.
        ob_init.USERConfig = OB_BOOT0_FROM_PIN | OB_nBOOT0_SET;
    }

    const bool programmed = (HAL_FLASHEx_OBProgram(&ob_init) == HAL_OK);
    if (programmed && launch) {
        // Reloads option bytes and resets. Does not return on success.
        HAL_FLASH_OB_Launch();
    }

    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    return programmed;
}

/**
 * Force a clean boot of the STM system-memory DFU bootloader by programming
 * the nSWBOOT0/nBOOT0 option bytes and launching them (resets the chip).
 *
 * Why not a software jump? Jumping to 0x1FFF0000 from the application does not
 * bring up USB DFU consistently (device fails enumeration). I think this
 * has to do with the state the device is left in when it enters dfu mode
 * from a software jump, anyway booting system memory via option bytes works
 * so lets do that.
 */
static void boot_system_memory_via_option_bytes(void) {
    (void)program_boot_option_bytes(true, true);
}

/**
 * If a previous DFU entry left the MCU configured to boot system memory,
 * restore normal main-flash boot so the next power cycle runs the application.
 * HAL_FLASH_OB_Launch() resets the chip when a change is applied.
 */
void system_hardware_handle_bootloader_request(void) {
    // FLASH_OPTR_nSWBOOT0 == 0 means boot mode comes from nBOOT0 option bit
    // (the state we enter for DFU). Restore pin/main-flash boot.
    if ((FLASH->OPTR & FLASH_OPTR_nSWBOOT0) == 0U) {
        (void)program_boot_option_bytes(false, true);
    }
}

/** PUBLIC FUNCTION IMPLEMENTATION */

void system_hardware_enter_bootloader(void) {
    // Drop the CDC interface so the host detaches before DFU re-enumerates.
    __HAL_RCC_USB_CLK_ENABLE();
    USB->BCDR &= (uint16_t)(~USB_BCDR_DPPU);
    USB->CNTR = (uint16_t)(USB_CNTR_FRES | USB_CNTR_PDWN);
    __HAL_RCC_USB_FORCE_RESET();
    __HAL_RCC_USB_RELEASE_RESET();
    __HAL_RCC_USB_CLK_DISABLE();

    const uint32_t disconnect_start = HAL_GetTick();
    while ((HAL_GetTick() - disconnect_start) < 50U) {
    }

    // This programs option bytes and resets into system-memory DFU. It does
    // not return on success. After a successful firmware update the app restores
    // main-flash boot via system_hardware_handle_bootloader_request().
    boot_system_memory_via_option_bytes();

    // If option-byte programming failed, fall back to a direct sysmem jump.
    // This path is known-unreliable for USB DFU on this board but is better
    // than hanging forever.
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    SCB->VTOR = 0;
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
    __enable_irq();
    asm volatile(
        "ldr r0, =%0\n"
        "ldr r1, [r0]\n"
        "msr msp, r1\n"
        "ldr r0, [r0, #4]\n"
        "bx  r0\n"
        :
        : "i"(SYSMEM_START)
        : "r0", "r1", "memory");
    __builtin_unreachable();
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
