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

bool startup_lock_pages(uint32_t start_page, uint32_t page_count) {
    // On the STM32F303, write protection is configured with a bitmask. Each
    // bit in the bitmask represents 2 pages, and each page is 2k. This means
    // that the inputs to this function must both be even!
    FLASH_OBProgramInitTypeDef init = {0};

    HAL_FLASHEx_OBGetConfig(&init);

    if((start_page & 0x1 )|| (page_count & 0x1) || (page_count == 0)) {
        // Cannot configure odd pages by themselves
        return false;
    }
    uint32_t mask = 0;
    for(uint32_t i = 0; i < (page_count / 2); ++i) {
        mask |= 1 << ((start_page / 2) + i);
    }

    // 1's indicate unlocked pages, so we invert for the check
    uint32_t set_pages = ~init.WRPPage;

    // If the mask is all already set, we can leave silently
    if((set_pages & mask) == mask) { 
        return true;
    }

    // We only want to update read protection
    init.OptionType = OPTIONBYTE_WRP;
    init.WRPState = OB_WRPSTATE_ENABLE;
    init.WRPPage = mask;

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
