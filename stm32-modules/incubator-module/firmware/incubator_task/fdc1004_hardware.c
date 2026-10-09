#include "firmware/fdc1004_hardware.h"

#define FDC1004_ADDRESS 0x50
#define FDC1004_RESET_REGISTER 0x0C
#define FDC1004_MEAS_CONFIG_BASE 0x08
#define FDC1004_MANUFACTURER_ID_REGISTER 0xFE
#define FDC1004_DEVICE_ID_REGISTER 0xFF
#define FDC1004_MANUFACTURER_ID 0x5449
#define FDC1004_DEVICE_ID 0x1004
#define FDC1004_RESET_BIT 0x8000
#define FDC1004_MAX_CHANNEL 3
#define FDC1004_MAX_CAPDAC 31
#define FDC1004_CHANNEL_SHIFT 13
#define FDC1004_CHB_SHIFT 10
#define FDC1004_CHB_DISABLED 0x7
#define FDC1004_CHB_CAPDAC 0x4
#define FDC1004_CAPDAC_SHIFT 5

static bool fdc1004_write_register(I2C_BUS bus, uint8_t reg, uint16_t value) {
    uint8_t data[2] = {(uint8_t)(value >> 8), (uint8_t)value};
    return hal_i2c_write(bus, FDC1004_ADDRESS << 1, reg, data, sizeof(data)) ==
           0;
}

static bool fdc1004_read_register(I2C_BUS bus, uint8_t reg, uint16_t *value) {
    uint8_t data[2] = {0};
    if (hal_i2c_read(bus, FDC1004_ADDRESS << 1, reg, data, sizeof(data)) != 0) {
        return false;
    }
    *value = ((uint16_t)data[0] << 8) | data[1];
    return true;
}

static bool fdc1004_check_identity(I2C_BUS bus) {
    uint16_t manufacturer_id = 0;
    uint16_t device_id = 0;
    return fdc1004_read_register(bus, FDC1004_MANUFACTURER_ID_REGISTER,
                                 &manufacturer_id) &&
           fdc1004_read_register(bus, FDC1004_DEVICE_ID_REGISTER, &device_id) &&
           manufacturer_id == FDC1004_MANUFACTURER_ID &&
           device_id == FDC1004_DEVICE_ID;
}

bool fdc1004_hardware_init(I2C_BUS bus, uint8_t channel, uint8_t capdac) {
    if (channel > FDC1004_MAX_CHANNEL || capdac > FDC1004_MAX_CAPDAC ||
        !fdc1004_check_identity(bus) ||
        !fdc1004_write_register(bus, FDC1004_RESET_REGISTER,
                                FDC1004_RESET_BIT)) {
        return false;
    }

    uint8_t chb = (capdac > 0) ? FDC1004_CHB_CAPDAC : FDC1004_CHB_DISABLED;
    uint16_t config = ((uint16_t)channel << FDC1004_CHANNEL_SHIFT) |
                      ((uint16_t)chb << FDC1004_CHB_SHIFT) |
                      ((uint16_t)capdac << FDC1004_CAPDAC_SHIFT);
    return fdc1004_write_register(bus, FDC1004_MEAS_CONFIG_BASE + channel,
                                  config);
}