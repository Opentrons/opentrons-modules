#include "liquid-height-tracker-module/fdc1004.hpp"

#include <array>
#include <iostream>

#include "firmware/i2c_comms.hpp"

constexpr int32_t FDC1004_SIGN_EXTENSION_MASK{static_cast<int32_t>(0xFF000000)};
constexpr uint32_t FDC1004_SIGN_BIT = 0x800000;
constexpr uint8_t FDC1004_FRACTIONAL_BITS{19};
constexpr double CAPDAC_STEP_PF{3.125};
constexpr uint8_t CAPDAC_MASK = 0x1F;
constexpr uint16_t HAL_ADDRESS = fdc1004::FDC1004::ADDRESS << 1;
constexpr uint16_t DATA_ADDR = 0xFF;
constexpr uint8_t CHB_DISABLED = 0b111;
constexpr uint16_t CAPDAC_SHIFT = 5;
constexpr uint16_t CHANNEL_SHIFT = 13;
constexpr uint16_t CHB_SHIFT = 10;
constexpr uint16_t RATE_SHIFT = 10;
constexpr uint8_t MAX_CHANNEL = 3;
constexpr uint8_t MAX_CAPDAC = 31;
constexpr uint16_t RATE_MASK = 0b11 << 10;
constexpr uint16_t REPEAT_MASK = 1 << 8;
constexpr uint16_t MEASUREMENT_TRIGGER_MASK = 0x00F0;
constexpr uint16_t MEASUREMENT_TRIGGER_SHIFT = 4;
constexpr auto MEASUREMENT_TRIGGER_BIT(uint8_t channel) {
    return 1 << (MEASUREMENT_TRIGGER_SHIFT + channel);
}
constexpr auto DONE_BIT_MASK(uint8_t channel) { return 1 << channel; }

using namespace fdc1004;

FDC1004::FDC1004(i2c::hardware::I2CBase* i2c) : _i2c(i2c) {}

auto FDC1004::write_register(uint8_t reg, uint16_t value) -> bool {
    std::array<uint8_t, 2> data = {static_cast<uint8_t>(value >> 8),
                                   static_cast<uint8_t>(value & DATA_ADDR)};
    return _i2c->i2c_write(HAL_ADDRESS, reg, data.data(), data.size()) == 0;
}

auto FDC1004::read_register(uint8_t reg, uint16_t& value) -> bool {
    std::array<uint8_t, 2> data = {};
    if (_i2c->i2c_read(HAL_ADDRESS, reg, data.data(), data.size()) != 0) {
        std::cerr << "Failed to read register 0x" << std::hex
                  << static_cast<int>(reg) << std::dec << std::endl;
        return false;
    }
    value =
        (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
    return true;
}

auto FDC1004::configure_single_ended(uint8_t channel, uint8_t capdac) -> bool {
    if (channel > MAX_CHANNEL || capdac > MAX_CAPDAC) {
        return false;
    }
    uint16_t config_value = (channel << CHANNEL_SHIFT) |
                            (CHB_DISABLED << CHB_SHIFT) |
                            (capdac << CAPDAC_SHIFT);
    return write_register(CONF_MEAS_BASE + channel, config_value);
}

auto FDC1004::trigger_measurement(uint8_t channel, Rate rate) -> bool {
    if (channel > MAX_CHANNEL) {
        return false;
    }
    uint16_t fdc_conf_value = {};
    if (!read_register(FDC_CONF, fdc_conf_value)) {
        return false;
    }
    fdc_conf_value &=
        ~(RATE_MASK | REPEAT_MASK | MEASUREMENT_TRIGGER_MASK);
    fdc_conf_value |=
        (static_cast<uint16_t>(rate) << RATE_SHIFT);  // Set new rate
    fdc_conf_value |= MEASUREMENT_TRIGGER_BIT(channel);
    return write_register(FDC_CONF, fdc_conf_value);
}

auto FDC1004::is_measurement_done(uint8_t channel) -> bool {
    if (channel > MAX_CHANNEL) {
        return false;
    }
    uint16_t fdc_conf_value = {};
    if (!read_register(FDC_CONF, fdc_conf_value)) {
        return false;
    }
    return (fdc_conf_value & DONE_BIT_MASK(channel)) != 0;
}

auto FDC1004::read_measurement(uint8_t channel, double& capacitance_pf)
    -> bool {
    if (channel > MAX_CHANNEL) {
        return false;
    }
    uint16_t msb = {};
    uint16_t lsb = {};
    if (!read_register(MEAS_MSB_BASE + channel * 2, msb) ||
        !read_register(MEAS_MSB_BASE + channel * 2 + 1, lsb)) {
        return false;
    }
    int32_t raw =
        (static_cast<int32_t>(msb) << 8) | (static_cast<int32_t>(lsb) >> 8);
    // Sign-extend from bit 23
    if ((raw & FDC1004_SIGN_BIT) != 0) {
        raw |= FDC1004_SIGN_EXTENSION_MASK;
    }
    // Read CAPDAC value for the channel
    uint16_t conf_value = {};
    if (!read_register(CONF_MEAS_BASE + channel, conf_value)) {
        return false;
    }
    uint8_t capdac = (conf_value >> CAPDAC_SHIFT) & CAPDAC_MASK;
    capacitance_pf = static_cast<double>(raw) / (1 << FDC1004_FRACTIONAL_BITS) +
                     static_cast<double>(capdac) * CAPDAC_STEP_PF;
    return true;
}