#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "firmware/atmosphere_pressure_sensor_policy.hpp"

namespace lps22df {
using atmosphere_pressure_sensor::hardware::AtmospherePressureSensorPolicy;
using i2c::hardware::RxTxReturn;

template <typename P>
concept LPS22DFPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

// 7-bit device is 5C if pin SDO is LOW
constexpr uint8_t DEVICE_ADDRESS = 0x5C << 1;
// address CTRL_REG2 to read pressure
constexpr uint8_t CTRL_REG2 = 0x11;
// pressure reading is 4 bytes, starting with status at 0x27
constexpr uint8_t PRESSURE_OUTPUT_REGISTER = 0x27;
// write bit 0 to CTRL REG 2 to read pressure once
constexpr uint8_t ONE_SHOT_PRESSURE_READ[1] = {0x01};
// pressure in hectoPascals is the 3 byte output value divided by the
// sensitivity
constexpr uint16_t SENSOR_SENSITIVITY = 4096;
// bit 0 in the status byte is for pressure reading available
constexpr uint8_t PRESSURE_READY_FLAG = 0x01;

constexpr uint8_t PRESSURE_FRAME_LEN = 10;
constexpr int DEFAULT_RETRIES = 3;

class LPS222DF {
  public:
    auto initialize(AtmospherePressureSensorPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    // TODO: separate sending the write pressure command from the read so
    // the task doesn't need to wait for the conversion
    auto read_pressure() -> std::optional<double> {
        _policy->i2c_write(DEVICE_ADDRESS, CTRL_REG2, ONE_SHOT_PRESSURE_READ,
                           1);
        for (int i = 0; i < (DEFAULT_RETRIES + 1); i++) {
            _policy->i2c_read(DEVICE_ADDRESS, PRESSURE_OUTPUT_REGISTER,
                              READ_BUFF, 4);
            _policy->sleep_ms(3);
            auto status_byte = READ_BUFF[0];
            pressure_reading_ready =
                static_cast<bool>(status_byte & PRESSURE_READY_FLAG);

            if (pressure_reading_ready) {
                break;
            }
        }

        auto pressure_hPa = convert_pressure(READ_BUFF);
        pressure_mbar = pressure_hPa;
        return pressure_hPa;
    }

    std::array<uint8_t, PRESSURE_FRAME_LEN> READ_BUFF = {0};
    std::array<uint8_t, PRESSURE_FRAME_LEN> WRITE_BUFF = {0};
    // 1 hectoPascal = mbar
    double pressure_mbar;
    bool pressure_reading_ready = false;

  private:
    AtmospherePressureSensorPolicy* _policy{nullptr};

    auto convert_pressure(uint8_t* sensor_output) -> double {
        auto pressure_read_bytes = {sensor_output[1], sensor_output[2],
                                    sensor_output[3]};
        // test that this is accurate
        auto pressure_read_counts = sensor_output[1] << 24 |
                                    sensor_output[2] << 16 |
                                    sensor_output[3] << 8;
        double pressure_hPa =
            static_cast<double>(pressure_read_counts / SENSOR_SENSITIVITY);
        return pressure_hPa
    }
};

}  // namespace lps22df