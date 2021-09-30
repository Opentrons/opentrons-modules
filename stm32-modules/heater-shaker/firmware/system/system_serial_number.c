#include <stdlib.h>

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_def.h"
#include "stm32f3xx_hal_flash.h"
#include "stm32f3xx_hal_flash_ex.h"

#include "system_serial_number.h"

static uint32_t PAGE_ADDRESS = 0x0805F800; //last page in flash memory, 0x0807F800 for 512K FLASH
static uint8_t ADDRESS_SIZE = 64;

bool system_set_serial_number(struct writable_serial* to_write) {
    FLASH_EraseInitTypeDef pageToErase = {.TypeErase = FLASH_TYPEERASE_PAGES, .PageAddress = PAGE_ADDRESS, .NbPages = 1};
    uint32_t pageErrorPtr = 0; //pointer to variable  that contains the configuration information on faulty page in case of error
    uint32_t ProgramAddress1 = PAGE_ADDRESS;
    uint32_t ProgramAddress2 = PAGE_ADDRESS + ADDRESS_SIZE;
    uint32_t ProgramAddress3 = PAGE_ADDRESS + (2 * ADDRESS_SIZE);

    HAL_StatusTypeDef status = HAL_FLASH_Unlock();
    if (status == HAL_OK) {
        status = HAL_FLASHEx_Erase(&pageToErase, &pageErrorPtr);
        if (status == HAL_OK) {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ProgramAddress1, to_write->contents[0]);
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ProgramAddress2, to_write->contents[1]);
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ProgramAddress3, to_write->contents[2]);
            if (status == HAL_OK) {
                status = HAL_FLASH_Lock();
            }
        }
    }
    return (status == HAL_OK);
}

uint64_t system_get_serial_number(uint8_t address) {
    uint32_t AddressToRead = PAGE_ADDRESS + (address * ADDRESS_SIZE);
    return *(uint64_t*)AddressToRead;
}
