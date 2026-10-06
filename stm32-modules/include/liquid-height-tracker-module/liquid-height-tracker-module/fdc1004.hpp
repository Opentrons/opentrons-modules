// Capacitive Sensor fdc1004 class object
// Constants aligned with ProtoCentral FDC1004 library:
// https://github.com/Protocentral/ProtoCentral_fdc1004_breakout
#pragma once
#include <cstdint>

#include "firmware/hardware_iface.hpp"

namespace fdc1004 {
class FDC1004 {
  public:
    static constexpr uint16_t ADDRESS = 0x50;

    // Sample rate field values for FDC_CONF[11:10]
    enum class Rate : uint16_t {
        SAMPLES_100HZ = 0x01,
        SAMPLES_200HZ = 0x02,
        SAMPLES_400HZ = 0x03
    };

    explicit FDC1004(i2c::hardware::I2CBase* i2c_bus);
    auto configure_single_ended(uint8_t channel, uint8_t capdac = 0) -> bool;
    auto trigger_measurement(uint8_t channel, Rate rate) -> bool;
    auto is_measurement_done(uint8_t channel) -> bool;
    auto read_measurement(uint8_t channel, double& capacitance_pf) -> bool;

  private:
    // Register addresses
    static constexpr uint8_t MEAS_MSB_BASE = 0x00;
    static constexpr uint8_t CONF_MEAS_BASE = 0x08;
    static constexpr uint8_t FDC_CONF = 0x0C;

    // CONF_MEASx bit fields
    static constexpr uint16_t CHA_SHIFT = 13;
    static constexpr uint16_t CHB_SHIFT = 10;
    static constexpr uint16_t CAPDAC_SHIFT = 5;
    static constexpr uint8_t CHB_DISABLED = 0x7;
    static constexpr uint8_t CHB_CAPDAC = 0x4;
    static constexpr uint8_t CAPDAC_MASK = 0x1F;

    // FDC_CONF bit fields
    static constexpr uint16_t RATE_SHIFT = 10;

    // Limits
    static constexpr uint8_t MAX_CHANNEL = 0x03;
    static constexpr uint8_t MAX_CAPDAC = 0x1F;

    // Conversion constants (ProtoCentral / upper 16-bit word)
    static constexpr double ATTOFARADS_UPPER_WORD = 457.0;
    static constexpr double FEMTOFARADS_CAPDAC = 3028.0;

    auto write_register(uint8_t reg, uint16_t value) -> bool;
    auto read_register(uint8_t reg, uint16_t& value) -> bool;
    i2c::hardware::I2CBase* _i2c;
};
}  // namespace fdc1004
