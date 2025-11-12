#pragma once

#include <array>
#include <cstdint>

#include "firmware/atmosphere_pressure_sensor_policy.hpp"

namespace lps22df {
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
// write bit 1 to CTRL REG 2 to read pressure once
constexpr uint8_t ONE_SHOT_PRESSURE_READ = 0x01;
// pressure in hectoPascals is the 3 byte output value divided by the
// sensitivity
constexpr uint16_t SENSOR_SENSITIVITY = 4096.0f;
// bit 0 in the status byte is for pressure reading available
constexpr uint8_t PRESSURE_READY_FLAG = 0x01;

constexpr uint8_t PRESSURE_FRAME_LEN = 10;

// Frame retry defaults
constexpr uint8_t DEFAULT_RETRIES = 3;
constexpr uint32_t DEFAULT_SLEEP_MS = 1;
using atmosphere_pressure_sensor::hardware::AtmospherePressureSensorPolicy;

class LPS222DF {
  public:
    auto initialize(AtmospherePressureSensorPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    auto read_pressure() -> double {
        bool pressure_reading_ready = false;
        auto len = prepare_cmd_frame(ONE_SHOT_PRESSURE_READ, nullptr, 0);
        _policy->i2c_write(DEVICE_ADDRESS, CTRL_REG2, WR_BUFF.data(), len);
        for (int i = 0; i < (DEFAULT_RETRIES + 1); i++) {
            // TODO: Needs at least 2ms for measurement
            // Find better way of doing this async.
            _policy->sleep_ms(2);
            _policy->i2c_read(DEVICE_ADDRESS, PRESSURE_OUTPUT_REGISTER,
                              RD_BUFF.data(), 4);
            auto status_byte = RD_BUFF[0];
            pressure_reading_ready =
                static_cast<bool>(status_byte & PRESSURE_READY_FLAG);

            if (pressure_reading_ready) {
                break;
            }
        }
        if (!pressure_reading_ready) {
            // raise an error here
        }

        pressure_hpa = parse_pressure(RD_BUFF.data());
        return pressure_hpa;
    }

  private:
    // Formulate a CMD frame
    auto prepare_cmd_frame(uint8_t cmd, const uint8_t* data, uint8_t len)
        -> uint8_t {
        if (len > PRESSURE_FRAME_LEN) {
            return 0;
        }
        // Add the header
        WR_BUFF[0] = cmd;
        // Copy data to the buffer starting from the header len.
        for (uint8_t i = 1; i < len; i++) {
            // NOLINTNEXTLINE
            WR_BUFF[i] = data[i - 1];
        }
        return len + 1;
    }

    static auto parse_pressure(const uint8_t* raw) -> double {
        // The pressure data are stored in three registers: PRESS_OUT_H (2Ah),
        // PRESS_OUT_L (29h), and PRESS_OUT_XL (28h). The value is expressed
        // as a 24-bit signed number (in two’s complement). To obtain the
        // pressure in hPa, take the complete 24-bit word and then divide by the
        // sensitivity 4096 LSB/hPa.
        int32_t pressure_lsb =
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            ((int32_t)raw[3] << 16) | ((int32_t)raw[2] << 8) | (int32_t)raw[1];

        // Convert from 24-bit twos complement to signed 32-bit
        pressure_lsb = ((int32_t)(pressure_lsb << 8)) >> 8;

        // Convert to hPa
        return static_cast<double>(pressure_lsb) / SENSOR_SENSITIVITY;
    }

    AtmospherePressureSensorPolicy* _policy{nullptr};
    std::array<uint8_t, PRESSURE_FRAME_LEN> RD_BUFF = {0};
    std::array<uint8_t, PRESSURE_FRAME_LEN> WR_BUFF = {0};

    double pressure_hpa = {0};
};

}  // namespace lps22df
