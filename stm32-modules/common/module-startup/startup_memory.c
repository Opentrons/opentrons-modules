
#include "startup_memory.h"

#include "startup_checks.h"
#include "startup_hal.h"

/** STATIC FUNCTION DECLARATIONS */
static bool erase_app(APP_SLOT_ENUM slot);

/** 
 * @brief Copy an image from src to dst. Assumes that src and dst are both
 * addresses in the Flash region, and that \p bytes is small enough to fit 
 * in the alloted space.
 */
static bool memory_copy_image(uint32_t src, uint32_t dst, uint32_t bytes);

/** Public function implementation */

bool memory_copy_backup_to_main() {
    startup_flash_init();
    if(!erase_app(APP_SLOT_MAIN)) {
        return false;
    }
    return memory_copy_image(
        slot_start_address(APP_SLOT_BACKUP),
        slot_start_address(APP_SLOT_MAIN),
        APPLICATION_MAX_SIZE);
}

bool memory_copy_main_to_backup() {
    startup_flash_init();
    if(!erase_app(APP_SLOT_BACKUP)) {
        return false;
    }
    return memory_copy_image(
        slot_start_address(APP_SLOT_MAIN),
        slot_start_address(APP_SLOT_BACKUP),
        APPLICATION_MAX_SIZE);
}

/** STATIC FUNCTION IMPLEMENTATIONS */

static bool erase_app(APP_SLOT_ENUM slot) {
    uint32_t start_page = slot_start_address(slot) / FLASH_PAGE_SIZE;
    uint32_t page_count = APPLICATION_MAX_SIZE / FLASH_PAGE_SIZE;
    
    HAL_FLASH_Unlock();
    bool ret = startup_erase_flash_pages(start_page, page_count);
    HAL_FLASH_Lock();

    return ret;
}

static bool memory_copy_image(uint32_t src, uint32_t dst, uint32_t bytes) {
    HAL_FLASH_Unlock();

    HAL_StatusTypeDef ret = HAL_ERROR;
    // All platforms support programming by doubleword, so we do that here.
    for(uint32_t offset = 0; offset < bytes; offset += 8) {
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, 
            dst + offset, 
            *(uint64_t *)(src + offset));

        if(ret != HAL_OK) {
            break;
        }
    }
    
    HAL_FLASH_Lock();

    return ret == HAL_OK;
}
