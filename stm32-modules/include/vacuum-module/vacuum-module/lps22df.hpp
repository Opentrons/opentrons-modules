#include <array>
#include <cstdint>
#include <optional>

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
uint8_t ONE_SHOT_PRESSURE_READ[1] = {0x01};
// pressure in hectoPascals is the 3 byte output value divided by the
// sensitivity
constexpr uint16_t SENSOR_SENSITIVITY = 4096;
// bit 0 in the status byte is for pressure reading available
constexpr uint8_t PRESSURE_READY_FLAG = 0x01;

constexpr uint8_t PRESSURE_FRAME_LEN = 4;
constexpr uint8_t WRITE_LEN = 2;

class LPS222DF {
  public:
    auto initialize(
        atmosphere_pressure_sensor::hardware::AtmospherePressureSensorPolicy*
            policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    auto read_pressure() -> std::optional<uint32_t> {
        pressure_reading_ready = false;

        _policy->i2c_write(DEVICE_ADDRESS, CTRL_REG2, ONE_SHOT_PRESSURE_READ,
                           1);
        _policy->sleep_ms(3);
        for (int i = 0; i < default_retries; i++) {
            _policy->i2c_read(DEVICE_ADDRESS, PRESSURE_OUTPUT_REGISTER,
                              READ_BUFF, 4);
            std::memcpy(this->sensor_output, this->READ_BUFF,
                        PRESSURE_FRAME_LEN);
            uint8_t status_byte = sensor_output[0];
            pressure_reading_ready =
                static_cast<bool>(status_byte & PRESSURE_READY_FLAG);

            if (pressure_reading_ready) {
                break;
            }
        }
        if (!presure_ready) {
            return std::nullopt
        }
        auto pressure_hPa = convert_pressure();
        return pressure_hPa;
    }
    uint8_t READ_BUFF[4] = {0x00};
    uint8_t sensor_output[4] = {0x00};
    bool pressure_reading_ready = false;

  private:
    atmosphere_pressure_sensor::hardware::AtmospherePressureSensorPolicy*
        _policy{nullptr};
    int default_retries = 5;

    auto convert_pressure() -> uint32_t {
        uint32_t pressure_read_counts = sensor_output[1] << 24 |
                                        sensor_output[2] << 16 |
                                        sensor_output[3] << 8;
        pressure_hPa = pressure_read_counts / SENSOR_SENSITIVITY;
        return pressure_hPa;
    }
};

}  // namespace lps22df
