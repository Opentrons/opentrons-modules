/**
 * @file startup_hal.h
 * @brief Provides HAL files specific to the F303
 */

#ifndef STARTUP_HAL_H_
#define STARTUP_HAL_H_

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_rcc.h"
#include "stm32g4xx_hal_cortex.h"
#include "stm32g4xx_hal_tim.h"

#define SYSMEM_ADDRESS            (0x1FFF0000)
#define BOOTLOADER_START_ADDRESS  (0x1FFF0004)
#define APPLICATION_START_ADDRESS (0x08008004)

#define DISABLE_CSS_FUNC() HAL_RCC_DisableLSECSS()

#endif /* STARTUP_HAL_H_ */

