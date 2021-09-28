#include <stdlib.h>

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_def.h"
#include "stm32f3xx_hal_flash.h"
#include "stm32f3xx_hal_flash_ex.h"

#include "system_serial_number.h"

static uint32_t PageAddress = 0x0805F800; //last page in flash memory, 0x0807F800 for 512K FLASH

bool system_set_serial_number(uint64_t to_write, uint8_t address) {
    FLASH_EraseInitTypeDef pageToErase = {.TypeErase = FLASH_TYPEERASE_PAGES, .PageAddress = PageAddress, .NbPages = 1};
    uint32_t pageErrorPtr = 0; //pointer to variable  that contains the configuration information on faulty page in case of error
    uint32_t ProgramAddress = PageAddress + (address * 64);

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status == HAL_OK) {
        status = HAL_FLASHEx_Erase(&pageToErase, &pageErrorPtr);
        if (status == HAL_OK) {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ProgramAddress, to_write);
            if (status == HAL_OK) {
                status = HAL_FLASH_Lock();
            }
        }
    }
    return (status == HAL_OK);
}

uint64_t system_get_serial_number(uint8_t address) {
    uint32_t AddressToRead = PageAddress + (address * 64);
    return *(uint64_t*)AddressToRead;
}
