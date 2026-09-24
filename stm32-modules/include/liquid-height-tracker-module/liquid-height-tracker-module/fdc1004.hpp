// Capacitive Sensor fdc1004 class object
#pragma once
#include <array>
#include <cstdint>

#include "firmware/hardware_iface.hpp"

namespace fdc1004 {
class FDC1004 {
  public:
    static constexpr uint16_t ADDRESS = 0x50;
    enum class Rate : uint16_t { SAMPLES_100HZ = 0b01, SAMPLES_400HZ = 0b11 };
    explicit FDC1004(i2c::hardware::I2CBase* i2c);
    auto who_am_i() -> bool;
    auto reset() -> bool;
    auto configure_single_ended(uint8_t channel, uint8_t capdac = 0) -> bool;
    auto trigger_measurement(uint8_t channel, Rate rate) -> bool;
    auto is_measurement_done(uint8_t channel) -> bool;
    auto read_measurement(uint8_t channel, double& capacitance_pf) -> bool;

  private:
    static constexpr uint8_t MEAS_MSB_BASE = 0x00;
    static constexpr uint8_t CONF_MEAS_BASE = 0x08;
    static constexpr uint8_t FDC_CONF = 0x0C;
    static constexpr uint8_t MANUFACTURER_ID_REG = 0xFE;
    static constexpr uint8_t DEVICE_ID_REG = 0xFF;
    static constexpr uint16_t MANUFACTURER_ID = 0x5449;
    static constexpr uint16_t DEVICE_ID = 0x1004;
    static constexpr uint16_t CHB_DISABLE = 0b111;

    auto write_register(uint8_t reg, uint16_t value) -> bool;
    auto read_register(uint8_t reg, uint16_t& value) -> bool;
    i2c::hardware::I2CBase* _i2c;
    std::array<uint8_t, 4> _capdac{};
};
}  // namespace fdc1004