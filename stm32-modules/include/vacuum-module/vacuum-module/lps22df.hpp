#pragma once

#include <array>
#include <cstdint>
#include <optional>

// #include "firmware/pressure_policy.hpp"

namespace lps22df {
using i2c::hardware::RxTxReturn;

template <typename P>
concept LPS22DFPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.is_device_ready(dev_addr) } -> std::same_as<bool>;
};

// 7-bit device is 5C if pin SDO is LOW
// address CTRL_REG2 to read pressure
constexpr uint8_t CTRL_REG2 = 0x11;
// pressure reading is 4 bytes, starting with status at 0x27
constexpr uint8_t PRESSURE_OUTPUT_REGISTER = 0x27;
// pressure in hectoPascals is the 3 byte output value divided by the
// sensitivity
constexpr uint16_t SENSOR_SENSITIVITY = 4096;
// bit 0 in the status byte is for pressure reading available
constexpr uint8_t PRESSURE_READY_FLAG = 0x01;
constexpr uint16_t DEFAULT_DEV_ADDR = 0x5D << 1;
constexpr uint8_t PRESSURE_FRAME_LEN = 4;
constexpr int FILTER_TAPS = 3;
constexpr int FILTERED_PRESSURE_LEN = 50;
// need to just tune these values based on testing outcomes
constexpr std::array<float, FILTER_TAPS> FILTER_VALUES = {0.6, 0.5, 0.5};
constexpr uint8_t WRITE_LEN = 2;
constexpr int DEFAULT_RETRIES = 5;

template <typename Policy>
requires LPS22DFPolicy<Policy>
class LPS222DF {
  public:
    LPS222DF(uint8_t device_address) : _device_address(device_address) {}
    auto initialize(Policy* policy, PressureSensorID sensor_id) -> bool {
        if (_policy == nullptr) {
            _policy = policy;
            _sensor_id = sensor_id;
        }
        return _policy->is_device_ready(_device_address << 1);
    }

    auto read_pressure() -> std::optional<double> {
        bool pressure_reading_ready = false;

        _policy->i2c_write(_device_address << 1, CTRL_REG2,
                           ONE_SHOT_PRESSURE_READ.data(), 1);
        for (int i = 0; i < DEFAULT_RETRIES; i++) {
            _policy->sleep_ms(3);
            _policy->i2c_read(_device_address << 1, PRESSURE_OUTPUT_REGISTER,
                              READ_BUFF.data(), 4);
            std::memcpy(sensor_output.data(), READ_BUFF.data(),
                        PRESSURE_FRAME_LEN);
            uint8_t status_byte = READ_BUFF[0];
            pressure_reading_ready =
                static_cast<bool>(status_byte & PRESSURE_READY_FLAG);

            if (pressure_reading_ready) {
                break;
            }
        }
        if (!pressure_reading_ready) {
            return std::nullopt;
        }
        double pressure_mbar = convert_pressure();
        return pressure_mbar;
    }

  private:
    // write bit 1 to CTRL REG 2 to read pressure once
    std::array<uint8_t, 1> ONE_SHOT_PRESSURE_READ = {0x01};
    std::array<uint8_t, PRESSURE_FRAME_LEN> READ_BUFF = {0x00};
    std::array<uint8_t, PRESSURE_FRAME_LEN> sensor_output = {0x00};
    // both of these act as FIFO buffers
    std::array<double, FILTER_TAPS> PRESSURE_BUFFER_MBAR = {0};
    std::array<double, FILTERED_PRESSURE_LEN> FILTERED_PRESSURE_MBAR = {0};
    int pressure_input_index = 0;
    int filtered_pressure_buffer_index = 0;
    uint8_t _device_address;
    PressureSensorID _sensor_id = PressureSensorID::ATM_PRESSURE;
    Policy* _policy{nullptr};

    auto convert_pressure() -> double {
        auto pressure_read_counts = static_cast<double>(
            sensor_output[1] << 16 | sensor_output[2] << 8 | sensor_output[3]);
        // hPa to mbar conversion is 1:1
        double pressure_mbar = pressure_read_counts / SENSOR_SENSITIVITY;
        return pressure_mbar;
    }
};

}  // namespace lps22df
