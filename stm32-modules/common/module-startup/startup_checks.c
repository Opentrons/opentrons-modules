#include "startup_checks.h"

#include <string.h>

#include "startup_hal.h"

#ifndef APPLICATION_START_ADDRESS
#error APPLICATION_START_ADDRESS must be defined
#endif

#ifndef APPLICATION_MAX_SIZE
#error APPLICATION_MAX_SIZE must be defined
#endif

#define INVALID_ADDR_MASK (0xFFFFFFF0)

#define APPLICATION_FOOTER_TOTAL_LENGTH (0x400)

// Take the start address and filter out to just the page
#define APPLICATION_VTABLE_START(address) (address & 0xFFFFF800)
// Application integrity region starts 0x200 from vtable
#define APPLICATION_INTEGRITY_REGION(address) \
    (APPLICATION_VTABLE_START(address) + 0x200)
// Actual application is stored 0x400 from vtable
#define APPLICATION_CRC_CALC_START_ADDRESS(address) \
    (APPLICATION_VTABLE_START(address) + APPLICATION_FOOTER_TOTAL_LENGTH)

#ifndef APPLICATION_FIRMWARE_NAME
#error APPLICATION_FIRMWARE_NAME must be defined
#endif

/** STATIC DATA */

typedef struct {
    bool init;
    CRC_HandleTypeDef crc;
} CheckHardware_t;

typedef struct __packed {
    uint32_t crc;
    uint32_t app_length;
    uint32_t app_start_address;
    char name;
} IntegrityRegion_t;

static CheckHardware_t check_hardware = {
    .init = false,
    .crc = {0}
};

static const char application_firmware_name[] __attribute__((used))  
    = APPLICATION_FIRMWARE_NAME;

/** STATIC FUNCTION DECLARATIONS */

static void init_hardware();
static void init_crc();
static uint32_t calculate_crc(uint32_t start, uint32_t count);

/** PUBLIC FUNCTION IMPLEMENTATIONS */

uint32_t slot_start_address(APP_SLOT_ENUM slot) {
    if(slot == APP_SLOT_BACKUP) {
        return APPLICATION_VTABLE_START( APPLICATION_START_ADDRESS ) +
                APPLICATION_MAX_SIZE;
    }
    // Default to the base slot
    return APPLICATION_VTABLE_START( APPLICATION_START_ADDRESS );
}

bool check_slot(APP_SLOT_ENUM slot) {
    if(slot > APP_SLOT_BACKUP) {
        return false;
    }

    return check_app_exists(slot) &&
           check_crc(slot) &&
           check_name(slot);
}

bool check_app_exists(APP_SLOT_ENUM slot) {
    const uint32_t boot_address = *(uint32_t*)(slot_start_address(slot) + 0x4);
    // If the address is mostly 1's, it is unprogrammed
    if( (boot_address & INVALID_ADDR_MASK) 
          == INVALID_ADDR_MASK) {
        return false;
    }
    // If the first bit of the address is 0, it is not a thumb instruction.
    if( (boot_address & 1) == 0) {
        return false;
    }
    return true;
}

bool check_crc(APP_SLOT_ENUM slot) {
    const IntegrityRegion_t  *const integrity_region = 
        (IntegrityRegion_t *)APPLICATION_INTEGRITY_REGION(slot_start_address(slot));
    const uint32_t crc_start = 
        APPLICATION_CRC_CALC_START_ADDRESS(slot_start_address(slot));
    const uint32_t crc_start_expected = 
        APPLICATION_CRC_CALC_START_ADDRESS(slot_start_address(APP_SLOT_MAIN));
    init_hardware();

    // Check that start address makes sense
    if(integrity_region->app_start_address 
            != crc_start_expected) {
        return false;
    }
    // Check that the length is programmed
    if(integrity_region->app_length == 0xFFFFFFFF) {
        return false;
    }
    // Check that the length is less than the max
    if(integrity_region->app_length > 
            APPLICATION_MAX_SIZE - APPLICATION_FOOTER_TOTAL_LENGTH) {
        return false;
    }
    // Run a CRC calc
    uint32_t crc = calculate_crc(
        crc_start, 
        integrity_region->app_length);
    
    if(crc != integrity_region->crc) {
        return false;
    }
    
    return true;
}

bool check_name(APP_SLOT_ENUM slot) {
    const IntegrityRegion_t  *const integrity_region = 
        (IntegrityRegion_t *)APPLICATION_INTEGRITY_REGION(slot_start_address(slot));
    const int namelen = strlen(application_firmware_name);

    return memcmp(
        application_firmware_name, 
        &integrity_region->name, 
        namelen) == 0;
}

bool check_backup_matches_main() {
    uint32_t start_main = slot_start_address(APP_SLOT_MAIN);
    uint32_t start_backup = slot_start_address(APP_SLOT_BACKUP);
    // Get the byte count from the main region
    const IntegrityRegion_t  *const main_integrity_region = 
        (IntegrityRegion_t *)APPLICATION_INTEGRITY_REGION(start_main);

    return memcmp(
        (void *)start_main,
        (void *)start_backup,
        main_integrity_region->app_length + APPLICATION_FOOTER_TOTAL_LENGTH
        ) == 0;
}

/** STATIC FUNCTION IMPLEMENTATIONS */

static void init_hardware() {
    if(check_hardware.init) {
        return;
    }

    init_crc();

    check_hardware.init = true;
}

static void init_crc() {
    check_hardware.crc.Instance = CRC;
    check_hardware.crc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
    check_hardware.crc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
    check_hardware.crc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_BYTE;
    check_hardware.crc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;
    check_hardware.crc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;

    (void) HAL_CRC_Init(&check_hardware.crc);
}

static uint32_t calculate_crc(uint32_t start, uint32_t count) {
    init_hardware();

    __HAL_CRC_DR_RESET(&check_hardware.crc);
    // We return the INVERTED calculated checksum in order to match
    // standard crc32 calculations
    return ~HAL_CRC_Calculate(
        &check_hardware.crc, 
        (uint32_t *)start,
        count);
}

/** OVERWRITTEN HAL FUNCTIONS */

/**
* @brief CRC MSP Initialization
* @param hcrc: CRC handle pointer
* @retval None
 */
void HAL_CRC_MspInit(CRC_HandleTypeDef* hcrc)
{
    __HAL_RCC_CRC_CLK_ENABLE();
}

/**
* @brief CRC MSP De-Initialization
* @param hcrc: CRC handle pointer
* @retval None
 */
void HAL_CRC_MspDeInit(CRC_HandleTypeDef* hcrc)
{
    __HAL_RCC_CRC_CLK_DISABLE();
}
