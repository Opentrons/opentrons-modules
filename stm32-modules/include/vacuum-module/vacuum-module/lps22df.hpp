#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "firmware/atmosphere_pressure_sensor_policy.hpp"

namespace lps22df {

template <typename P>
concept LPS22DFPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

constexpr uint8_t DEVICE_ADDRESS = 0xB8;
// address CTRL_REG2 to read pressure
constexpr uint8_t CTRL_REG2 = 0x11;
// pressure reading is 4 bytes, starting with status at 0x27
constexpr uint8_t PRESSURE_OUTPUT_REGISTER = 0x27;
// write bit 1 to CTRL REG 2 to read pressure once
constexpr uint8_t ONE_SHOT_PRESSURE_READ[1] = {0x01};
// pressure in hectoPascals is the 3 byte output value divided by the
// sensitivity
constexpr uint16_t SENSOR_SENSITIVITY = 4096;
// bit 0 in the status byte is for pressure reading available
constexpr uint8_t PRESSURE_READY_FLAG = 0x01;

class LPS222DF {
  public:
    auto initialize(atmosphere_pressure_sensor::hardware::AtmospherePressureSensorPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    auto read_pressure(int retries) -> uint16_t {
        uint8_t read_buff[4] = {0x00};
        bool pressure_reading_ready = false;

        _policy->i2c_write(DEVICE_ADDRESS, CTRL_REG2, ONE_SHOT_PRESSURE_READ,
                           1);
        _policy->sleep_ms(5);
        for (int i = 0; i < retries; i++) {
            _policy->i2c_read(DEVICE_ADDRESS, PRESSURE_OUTPUT_REGISTER,
                              read_bufff, 4);
            auto status_byte = read_buff[0];
            pressure_reading_ready =
                static_cast<bool>(status_byte & PRESSURE_READY_FLAG);

            if (pressure_reading_ready) {
                break;
            }
        }
        if (!pressure_reading_ready) {
            // raise an error here
        }

        auto pressure_hPa = convert_pressure(read_buff);
        return pressure_hPa
    }

  private:
    atmosphere_pressure_sensor::hardware::AtmospherePressureSensorPolicy* _policy{nullptr};

    auto convert_pressure(uint8_t* sensor_output) -> uint16_t {
        auto pressure_read_bytes = {sensor_output[1], sensor_output[2],
                                    sensor_output[3]};
        // test that this is accurate
        auto pressure_read_counts =
            sensor_output[1] << 16 | sensor_output[2] << 8 | sensor_output[3];
        auto pressure_hPa = pressure_read_counts / SENSOR_SENSITIVITY;
        return pressure_hPa
    }
};

}  // namespace lps22df