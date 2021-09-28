#include <stdlib.h>

#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_def.h"
#include "stm32f3xx_hal_flash.h"
#include "stm32f3xx_hal_flash_ex.h"

#include "system_serial_number.h"

static uint32_t PageAddress = 0x0805F800; //last page in flash memory, 0x0807F800 for 512K FLASH
//static uint8_t Serial_Number[] = (uint8_t*)0x0805F800;

bool system_set_serial_number(uint64_t to_write, uint8_t page) {
    FLASH_EraseInitTypeDef pageToErase = {.TypeErase = FLASH_TYPEERASE_PAGES, .PageAddress = PageAddress, .NbPages = 1};
    //FLASH_EraseInitTypeDef *pagePtr = &pageToErase;
    uint32_t pageErrorPtr = 0; //pointer to variable  that contains the configuration information on faulty page in case of error
    uint32_t ProgramAddress = PageAddress + (page * 64);

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

uint64_t system_get_serial_number(uint8_t page) {
    uint32_t AddressToRead = PageAddress + (page * 64);
    return *(uint64_t*)AddressToRead;
}

//HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError)
//HAL_StatusTypeDef HAL_FLASH_Program(uint32_t TypeProgram, uint32_t Address, uint64_t Data)
