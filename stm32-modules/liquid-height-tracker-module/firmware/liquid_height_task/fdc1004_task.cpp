// Key facts you need from the datasheet:
//     -I2C address is fixed: 0x50
//     -Registers: MEAS1_MSB..MEAS4_LSB = 0x00-0x07, CONF_MEAS1..4 = 0x08-0x0B,
//     FDC_CONF = 0x0C,
//         MANUFACTURER_ID = 0xFE (expect 0x5449), DEVICE_ID = 0xFF (expect
//         0x1004)
//     -CONF_MEASx (16-bit): bits [15:13] = CHA (channel, 0-3),
//         bits [12:10] = CHB (0b111 = disabled = single-ended),
//         bits [9:5] = CAPDAC (0-31)
//     -FDC_CONF: bit 15 = reset, bits [11:10] = rate (01=100Hz, 10=200Hz,
//         11=400Hz), bits [3:0] = trigger/done flags for channels 1-4 (bit
//         3-channel)
//     -Result conversion: combine MSB<<8 | (LSB>>8) into a signed 24-bit value,
//         sign-extend from bit 23, then capacitance_pF = raw / 2^19 + capdac
//         * 3.125

#include <iostream>

#include "firmware/i2c_comms.hpp"
#include "liquid-height-tracker-module/errors.hpp"
#include "liquid-height-tracker-module/fdc1004.hpp"

using namespace fdc1004;

constexpr uint16_t RESET_BIT = 0x8000;
constexpr uint16_t ADDRESS = 0x50;
constexpr uint8_t CHB_DISABLED = 0b111;
constexpr uint16_t CAPDAC_SHIFT = 5;
constexpr uint8_t MAX_CHANNEL = 3;
constexpr uint8_t MAX_CAPDAC = 31;
constexpr uint16_t RATE_MASK = 0b11 << 10;
constexpr uint16_t DONE_BIT_MASK(uint8_t channel) { return 1 << (3 - channel); }

auto FDC1004::write_register(uint8_t reg, uint16_t value) -> bool {
    std::array<uint8_t, 2> data = {static_cast<uint8_t>(value >> 8),
                                   static_cast<uint8_t>(value & 0xFF)};
    return _i2c->i2c_write(ADDRESS, reg, data.data(), data.size()) == 0;
}

auto FDC1004::read_register(uint8_t reg, uint16_t& value) -> bool {
    std::array<uint8_t, 2> data;
    if (_i2c->i2c_read(ADDRESS, reg, data.data(), data.size()) != 0) {
        std::cerr << "Failed to read register 0x" << std::hex
                  << static_cast<int>(reg) << std::dec << std::endl;
        return false;
    }
    value =
        (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
    return true;
}

auto FDC1004::who_am_i() -> bool {
    uint16_t manufacturer_id;
    uint16_t device_id;
    if (!read_register(MANUFACTURER_ID_REG, manufacturer_id) ||
        !read_register(DEVICE_ID_REG, device_id)) {
        std::cerr << "Failed to read manufacturer or device ID" << std::endl;
        return false;
    }
    if (manufacturer_id != MANUFACTURER_ID || device_id != DEVICE_ID) {
        std::cerr << "Unexpected manufacturer or device ID: "
                  << "Manufacturer ID: 0x" << std::hex << manufacturer_id
                  << ", Device ID: 0x" << device_id << std::dec << std::endl;
        return false;
    }
    return true;
}

auto FDC1004::reset() -> bool { return write_register(FDC_CONF, RESET_BIT); }

auto FDC1004::configure_single_ended(uint8_t channel, uint8_t capdac) -> bool {
    if (channel > MAX_CHANNEL || capdac > MAX_CAPDAC) {
        return false;
    }
    uint16_t config_value =
        (channel << 13) | (CHB_DISABLED << 10) | (capdac << CAPDAC_SHIFT);
    return write_register(CONF_MEAS_BASE + channel, config_value);
}

auto FDC1004::trigger_measurement(uint8_t channel, Rate rate) -> bool {
    if (channel > MAX_CHANNEL) {
        return false;
    }
    uint16_t fdc_conf_value;
    if (!read_register(FDC_CONF, fdc_conf_value)) {
        return false;
    }
    fdc_conf_value &= ~RATE_MASK;                           // Clear rate bits
    fdc_conf_value |= (static_cast<uint16_t>(rate) << 10);  // Set new rate
    fdc_conf_value |=
        DONE_BIT_MASK(channel);  // Set trigger bit for the channel
    return write_register(FDC_CONF, fdc_conf_value);
}

auto FDC1004::is_measurement_done(uint8_t channel) -> bool {
    if (channel > MAX_CHANNEL) {
        return false;
    }
    uint16_t fdc_conf_value;
    if (!read_register(FDC_CONF, fdc_conf_value)) {
        return false;
    }
    return (fdc_conf_value & (1 << (MAX_CHANNEL - channel))) ==
           0;  // Check if done bit is cleared
}

auto FDC1004::read_measurement(uint8_t channel, double& capacitance_pf)
    -> bool {
    if (channel > MAX_CHANNEL) {
        return false;
    }
    uint16_t msb, lsb;
    if (!read_register(MEAS_MSB_BASE + channel * 2, msb) ||
        !read_register(MEAS_MSB_BASE + channel * 2 + 1, lsb)) {
        return false;
    }
    int32_t raw =
        (static_cast<int32_t>(msb) << 8) | (static_cast<int32_t>(lsb) >> 8);
    // Sign-extend from bit 23
    if (raw & 0x800000) {
        raw |= ~0xFFFFFF;
    }
    // Read CAPDAC value for the channel
    uint16_t conf_value;
    if (!read_register(CONF_MEAS_BASE + channel, conf_value)) {
        return false;
    }
    uint8_t capdac = (conf_value >> 5) & 0x1F;
    capacitance_pf = static_cast<double>(raw) / (1 << 19) + capdac * 3.125;
    return true;
}