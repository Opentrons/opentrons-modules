#include "startup_hal.h"

#include <stdbool.h>

#include "stm32g4xx_hal_flash.h"
#include "stm32g4xx_hal_flash_ex.h"

void startup_flash_init() {
    __HAL_RCC_FLASH_CLK_ENABLE();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_SR_ERRORS);
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

bool startup_lock_pages(uint32_t start_page, uint32_t page_count) {
    // On the STM32G491, write protection is configured with a two page
    // indices, a start and end. The region is inclusive of the end page,
    // so to protect one page you would have to set both start and end
    // to the same value.
    FLASH_OBProgramInitTypeDef init = {0};
    init.WRPArea = OB_WRPAREA_BANK1_AREAA; // Get Area A info

    HAL_FLASHEx_OBGetConfig(&init);

    if((page_count == 0)) {
        // Cannot configure 1 page at a time
        return false;
    }

    uint32_t end_page = start_page + page_count - 1;

    // If the mask is all already set, we can leave silently
    if(init.WRPStartOffset == start_page && 
       init.WRPEndOffset == end_page) { 
        return true;
    }

    // We only want to update read protection
    init.OptionType = OPTIONBYTE_WRP;
    init.WRPArea = OB_WRPAREA_BANK1_AREAA;
    init.WRPStartOffset = start_page;
    init.WRPEndOffset = end_page;

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    bool ret = HAL_FLASHEx_OBProgram(&init) == HAL_OK;
    // This restarts the device
    if(ret) {
        HAL_FLASH_OB_Launch();
    }
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    // If we were succesful, we shouldn't return at all
    return false;
}
