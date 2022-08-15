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

// Take the start address and filter out to just the page
#define APPLICATION_VTABLE_START (APPLICATION_START_ADDRESS & 0xFFFFF000)
// Application integrity region starts 0x200 from 
#define APPLICATION_INTEGRITY_REGION (APPLICATION_VTABLE_START + 0x200)
#define APPLICATION_CRC_CALC_START_ADDRESS (APPLICATION_VTABLE_START + 0x400)

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

static const IntegrityRegion_t  *const integrity_region = (IntegrityRegion_t *)APPLICATION_INTEGRITY_REGION;

static const char application_firmware_name[] __attribute__((used))  
    = APPLICATION_FIRMWARE_NAME;

/** STATIC FUNCTION DECLARATIONS */

static void init_hardware();
static void init_crc();
static uint32_t calculate_crc(uint32_t start, uint32_t count);

/** PUBLIC FUNCTION IMPLEMENTATIONS */

bool check_app_exists() {
    const uint32_t boot_address = *(uint32_t*)APPLICATION_START_ADDRESS;
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

bool check_crc() {
    init_hardware();

    // Check that start address makes sense
    if(integrity_region->app_start_address 
            != APPLICATION_CRC_CALC_START_ADDRESS) {
        return false;
    }
    // Check that the length is programmed
    if(integrity_region->app_length == 0xFFFFFFFF) {
        return false;
    }
    // Run a CRC calc
    uint32_t crc = calculate_crc(
        integrity_region->app_start_address, 
        integrity_region->app_length);
    
    if(crc != integrity_region->crc) {
        return false;
    }
    
    return true;
}

bool check_name() {
    const int namelen = strlen(application_firmware_name);

    return memcmp(
        application_firmware_name, 
        &integrity_region->name, 
        namelen) == 0;
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
