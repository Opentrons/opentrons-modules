#include "startup_hal.h"

#include <stdbool.h>

#include "stm32g4xx_hal_flash.h"
#include "stm32g4xx_hal_flash_ex.h"

void startup_flash_init() {
    __HAL_RCC_FLASH_CLK_ENABLE();
}

// Each target has a different method to set the page for erasing
bool startup_erase_flash_pages(uint32_t start_page, uint32_t page_count) {
    FLASH_EraseInitTypeDef config = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_1,
        .Page = start_page,
        .NbPages = page_count,
    };
    uint32_t err = 0;
    if (HAL_FLASHEx_Erase(&config, &err) == HAL_OK) {
        return err == 0xFFFFFFFF;
    }
    return false;
}
