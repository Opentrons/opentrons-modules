// Capacitive Sensor fdc1004 class object
#pragma once
#include <array>
#include <cstdint>
#include "firmware/hardware_iface.hpp"

namespace fdc1004 {
class FDC1004 {
    public:
        static constexpr uint16_t ADDRESS = 0x50;
        enum class Rate : unint16_t { SAMPLES_100HZ = 0b01, SAMPLES_400HZ = 0b11};
        explict FDC1004(i2c::hardware::I2CBASE* i2c);
        auto who_am_i() -> bool;
        auto reset() -> bool;
        auto configure_single_ended(uint8_t channel, uint8_t capdac = 0) -> bool;
        auto trigger_measurement(uint8_t channel, Rate rate) -> bool;
        auto is_measurment_done(uint8_t channel, double& capacitance_pf) -> bool;

    private:
        auto write_register(uint8_t reg, uint16_t value) -> bool;
        auto read_register(uint8_t reg, uint16_t& value) -> bool;
        i2c::hardware::I2CBase* _i2c;
        std::array<uint8_t, 4> _capdac{};
};
}