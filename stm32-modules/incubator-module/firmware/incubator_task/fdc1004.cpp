#include "incubator-module/fdc1004.hpp"

#include <array>

#include "firmware/i2c_comms.hpp"

constexpr uint16_t HAL_ADDRESS = fdc1004::FDC1004::ADDRESS << 1;
constexpr uint16_t DATA_ADDR = 0xFF;
// FDC_CONF: MEAS1_EN is bit 7 ... MEAS4_EN is bit 4; DONE_1 is bit 3 ... DONE_4
// is bit 0
constexpr auto MEASUREMENT_TRIGGER_BIT(uint8_t measurement) {
    return static_cast<uint16_t>(1U << (7U - measurement));
}
constexpr auto DONE_BIT_MASK(uint8_t measurement) {
    return static_cast<uint16_t>(1U << (3U - measurement));
}

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
    // Match ProtoCentral: enable CAPDAC on CHB when an offset is requested.
    const uint8_t chb = (capdac > 0) ? CHB_CAPDAC : CHB_DISABLED;
    uint16_t config_value = static_cast<uint16_t>(channel << CHA_SHIFT) |
                            static_cast<uint16_t>(chb << CHB_SHIFT) |
                            static_cast<uint16_t>(capdac << CAPDAC_SHIFT);
    return write_register(CONF_MEAS_BASE + channel, config_value);
}

auto FDC1004::trigger_measurement(uint8_t channel, Rate rate) -> bool {
    if (channel > MAX_CHANNEL) {
        return false;
    }
    // Write a fresh FDC_CONF: rate, repeat=0, and the matching MEASn enable
    // bit.
    uint16_t fdc_conf_value =
        static_cast<uint16_t>(static_cast<uint16_t>(rate) << RATE_SHIFT) |
        MEASUREMENT_TRIGGER_BIT(channel);
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
    // ProtoCentral uses the upper 16-bit word as a signed value.
    // LSB is still read to complete the measurement readout sequence.
    const int16_t raw = static_cast<int16_t>(msb);
    (void)lsb;

    uint16_t conf_value = {};
    if (!read_register(CONF_MEAS_BASE + channel, conf_value)) {
        return false;
    }
    const uint8_t capdac =
        static_cast<uint8_t>((conf_value >> CAPDAC_SHIFT) & CAPDAC_MASK);

    // capacitance_pf = (457 aF * raw) / 1e6 + (3028 fF * capdac) / 1000
    capacitance_pf =
        (ATTOFARADS_UPPER_WORD * static_cast<double>(raw)) / 1000000.0 +
        (FEMTOFARADS_CAPDAC * static_cast<double>(capdac)) / 1000.0;
    return true;
}
