/**
 * @file startup_hal.h
 * @brief Provides HAL files specific to the F303
 */

#ifndef STARTUP_HAL_H_
#define STARTUP_HAL_H_

#include <stdbool.h>

#include "startup_system_stm32f3xx.h"
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_conf.h"
#include "stm32f3xx_hal_cortex.h"
#include "stm32f3xx_hal_crc.h"
#include "stm32f3xx_hal_rcc.h"

#define SYSMEM_ADDRESS (0x1FFFD800)
#define BOOTLOADER_START_ADDRESS (0x1FFFD804)
#define APPLICATION_START_ADDRESS (0x08008004)

// 238K for application
#define APPLICATION_MAX_SIZE (0x400 * 238)

#define DISABLE_CSS_FUNC() HAL_RCC_DisableCSS()

// No flash init needed on F303
#define startup_flash_init() (void)(0)

// Each target has a different method to set the page for erasing
bool startup_erase_flash_pages(uint32_t start_page, uint32_t page_count);

// Each target has a different method to lock pages
bool startup_lock_pages(uint32_t start_page, uint32_t page_count);

#endif /* STARTUP_HAL_H_ */
