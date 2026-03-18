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
// address CTRL_REG2 to initialize data read
constexpr uint8_t CTRL_REG2 = 0x11;
// pressure + temp reading is 5 bytes, starting with status at 0x27
constexpr uint8_t DATA_OUTPUT_REGISTER = 0x27;
// write 0x11 to CTRL REG 2 to read pressure and temperature once
constexpr uint8_t ONE_SHOT_DATA_READ = 0x11;
// pressure in hectoPascals is the 3 byte output value divided by the
// sensitivity
constexpr uint16_t SENSOR_SENSITIVITY_PRESSURE = 4096;
// temperature in celcius is the 2 byte output divided by the sensitivity.
constexpr uint16_t SENSOR_SENSITIVITY_TEMPERATURE = 100;
// bit 0 in the status byte is for pressure reading available
constexpr uint8_t PRESSURE_READY_FLAG = 0x01;
// bit 1 in the status byte is for temperature reading available
constexpr uint8_t TEMP_READY_FLAG = 0x02;
constexpr uint8_t DATA_FRAME_LEN = 6;
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

    auto read_pressure(bool reset_filter = false) -> double {
        static_cast<void>(reset_filter);
        auto ok = read_data();
        if (ok) {
            return pressure_hpa;
        }
        return -1;
    }

    auto read_temperature() -> double {
        auto ok = read_data();
        if (ok) {
            return temperature;
        }
        return -1;
    }

    [[nodiscard]] auto get_pressure() const -> double { return pressure_hpa; }
    [[nodiscard]] auto get_temperature() const -> double { return temperature; }

  private:
    // Formulate a CMD frame
    auto prepare_cmd_frame(uint8_t cmd, const uint8_t* data, uint8_t len)
        -> uint8_t {
        if (len > DATA_FRAME_LEN) {
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

    auto read_data() -> bool {
        bool pres_ready = false;
        bool temp_ready = false;
        auto len = prepare_cmd_frame(ONE_SHOT_DATA_READ, nullptr, 0);
        _policy->i2c_write(device_address << 1, CTRL_REG2, WR_BUFF.data(), len);
        for (int i = 0; i < (DEFAULT_RETRIES + 1); i++) {
            // NOTE: Needs at least 7ms for conversion
            // Find better way of doing this async.
            _policy->sleep_ms(DEFAULT_SLEEP_MS);
            _policy->i2c_read(device_address << 1, DATA_OUTPUT_REGISTER,
                              RD_BUFF.data(), DATA_FRAME_LEN);
            auto status_byte = RD_BUFF[0];
            pres_ready = static_cast<bool>(status_byte & PRESSURE_READY_FLAG);
            temp_ready = static_cast<bool>(status_byte & TEMP_READY_FLAG);
            if (pres_ready || temp_ready) {
                break;
            }
        }

        if (pres_ready) {
            pressure_hpa = parse_pressure(RD_BUFF.data());
        }
        if (temp_ready) {
            temperature = parse_temperature(RD_BUFF.data());
        }

        return pres_ready && temp_ready;
    }

    static auto parse_pressure(const uint8_t* raw) -> double {
        // The pressure data is stored in three registers: PRESS_OUT_H (2Ah),
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
        return static_cast<double>(pressure_lsb) / SENSOR_SENSITIVITY_PRESSURE;
    }

    static auto parse_temperature(const uint8_t* raw) -> double {
        // The temperature is are stored in two registers: TEMP_OUT_L (2Bh),
        // and TEMP_OUT_H (2Ch). The value is expressed as a 16-bit signed
        // number (in two’s complement). To obtain the temperature in Celcius,
        // take the complete 16-bit and then divide by the sensitivity 100
        // LSB/C.

        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const uint16_t temp_u16 = (static_cast<uint16_t>(raw[5]) << 8) |
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                                  static_cast<uint16_t>(raw[4]);
        const auto temp_lsb = static_cast<int16_t>(temp_u16);

        // Convert to Celcius
        return static_cast<double>(temp_lsb) / SENSOR_SENSITIVITY_TEMPERATURE;
    }

    Policy* _policy{nullptr};
    std::array<uint8_t, DATA_FRAME_LEN> RD_BUFF = {0};
    std::array<uint8_t, DATA_FRAME_LEN> WR_BUFF = {0};
    PressureSensorID _sensor_id{};
    uint8_t device_address{};

    double pressure_hpa = {0};
    double temperature = {0};
};

}  // namespace lps22hb
