#include "startup_hal.h"

#include "stm32f3xx_hal_flash.h"
#include "stm32f3xx_hal_flash_ex.h"

// Each target has a different method to set the page for erasing
bool startup_erase_flash_pages(uint32_t start_page, uint32_t page_count) {
    FLASH_EraseInitTypeDef config = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .PageAddress = start_page * FLASH_PAGE_SIZE,
        .NbPages = page_count
    };
    uint32_t err = 0;
    if (HAL_FLASHEx_Erase(&config, &err) == HAL_OK) {
        return err == 0xFFFFFFFF;
    }
    return false;
}
