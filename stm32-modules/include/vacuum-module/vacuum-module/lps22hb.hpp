#pragma once

#include <array>
#include <cstdint>

namespace lps22hb {
using i2c::hardware::RxTxReturn;

template <typename P>
concept LPS22HBPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.is_device_ready(dev_addr) } -> std::same_as<bool>;
};

// 7-bit device is 5D if pin SDO is HIGH
constexpr uint8_t DEVICE_ADDRESS = 0x5D;
// address CTRL_REG2 to read pressure
constexpr uint8_t CTRL_REG2 = 0x11;
// pressure reading is 4 bytes, starting with status at 0x27
constexpr uint8_t PRESSURE_OUTPUT_REGISTER = 0x27;
// write 0x11 to CTRL REG 2 to read pressure once
constexpr uint8_t ONE_SHOT_PRESSURE_READ = 0x11;
// pressure in hectoPascals is the 3 byte output value divided by the
// sensitivity
constexpr uint16_t SENSOR_SENSITIVITY = 4096;
// bit 0 in the status byte is for pressure reading available
constexpr uint8_t PRESSURE_READY_FLAG = 0x01;

constexpr uint8_t PRESSURE_FRAME_LEN = 10;

// Frame retry defaults
constexpr uint8_t DEFAULT_RETRIES = 3;
constexpr uint32_t DEFAULT_SLEEP_MS = 10;

template <typename Policy>
requires LPS22HBPolicy<Policy>
class LPS22HB {
  public:
    LPS22HB(uint8_t dev_address = DEVICE_ADDRESS)
        : device_address{dev_address} {}

    auto initialize(Policy* policy, PressureSensorID sensor_id) -> bool {
        auto ok = false;
        if (_policy == nullptr) {
            _policy = policy;
            _sensor_id = sensor_id;

            // check device status
            ok = _policy->is_device_ready(device_address << 1);
            ok = read_pressure() > 0;
        }

        return ok;
    }

    [[nodiscard]] auto get_pressure() const -> double { return pressure_hpa; }

    auto read_pressure() -> double {
        bool pressure_reading_ready = false;
        auto len = prepare_cmd_frame(ONE_SHOT_PRESSURE_READ, nullptr, 0);
        _policy->i2c_write(device_address << 1, CTRL_REG2, WR_BUFF.data(), len);
        for (int i = 0; i < (DEFAULT_RETRIES + 1); i++) {
            // NOTE: Needs at least 7ms for conversion
            // Find better way of doing this async.
            _policy->sleep_ms(DEFAULT_SLEEP_MS);
            _policy->i2c_read(device_address << 1, PRESSURE_OUTPUT_REGISTER,
                              RD_BUFF.data(), 4);
            auto status_byte = RD_BUFF[0];
            pressure_reading_ready =
                static_cast<bool>(status_byte & PRESSURE_READY_FLAG);

            if (pressure_reading_ready) {
                break;
            }
        }

        // Error state
        if (!pressure_reading_ready) {
            return -1;
        }

        pressure_hpa = parse_pressure(RD_BUFF.data());
        return pressure_hpa;
    }

    // TODO: get rid of this and add a visitor to the pressure_task call to this
    // so we dont have to call a dumb function here
    auto solid_state_target_pressure(double target_pressure, int num_samples,
                                     double tolerance) -> bool {
        static_cast<void>(target_pressure);
        static_cast<void>(num_samples);
        static_cast<void>(tolerance);
        return false;
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

    Policy* _policy{nullptr};
    std::array<uint8_t, PRESSURE_FRAME_LEN> RD_BUFF = {0};
    std::array<uint8_t, PRESSURE_FRAME_LEN> WR_BUFF = {0};
    PressureSensorID _sensor_id{};
    uint8_t device_address{};

    double pressure_hpa = {0};
};

}  // namespace lps22hb
