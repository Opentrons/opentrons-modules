/**
 * @file startup_hal.h
 * @brief Provides HAL files specific to the G491
 */

#ifndef STARTUP_HAL_H_
#define STARTUP_HAL_H_

#include <stdbool.h>

#include "startup_system_stm32g4xx.h"
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_conf.h"
#include "stm32g4xx_hal_cortex.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal_tim.h"

#define SYSMEM_ADDRESS (0x1FFF0000)
#define BOOTLOADER_START_ADDRESS (0x1FFF0004)
#define APPLICATION_START_ADDRESS (0x08008004)

// 238K for application
#define APPLICATION_MAX_SIZE (0x400 * 238)

#define DISABLE_CSS_FUNC() HAL_RCC_DisableLSECSS()

// Needs to init the RCC
void startup_flash_init();

// Each target has a different method to set the page for erasing
bool startup_erase_flash_pages(uint32_t start_page, uint32_t page_count);

#endif /* STARTUP_HAL_H_ */
