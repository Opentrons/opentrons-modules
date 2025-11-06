#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace vacuum_pressure_sensor {

template <typename P>
concept MPRPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                             uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

constexpr uint8_t DEV_ADDRESS = 0x30;
constexpr uint8_t MEASURE_PRESSURE_COMMAND = 0xAA;
constexpr uint8_t STATUS_BUSY_FLAG = 0x20;
constexpr uint16_t OUTPUT_MAX = 15099494;
constexpr uint16_t OUTPUT_MIN = 1677722;
constexpr uint16_t OUTPUT_RANGE_COUNTS = OUTPUT_MAX - OUTPUT_MIN;
// range for this particular model is 0-25 PSI
constexpr uint16_t PRESSURE_RANGE_PSI = 25;

struct StatusByte {
    uint8_t power_indication;
    uint8_t busy_flag;
    uint8_t error_flag;
    uint8_t math_saturation;
};

class MPRLL0025PA00001 {
  public:
    auto initialize(MPRPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    auto read_pressure(int retries) -> uint16_t {
        uint8_t read_buff[4] = {0x00};
        bool sensor_busy = true;

        policy->i2c_write(0x18 << 1, 0xAA, read_buff, 0);

        for (int i = 0; i < retries; i++) {
            policy->i2c_master_read(0x18 << 1, read_buff, 4);
            auto status_byte = read_buff[0];
            sensor_busy = static_cast<bool>(status_byte & STATUS_BUSY_FLAG);

            if (!sensor_busy) {
                break;
            }
        }

        auto pressure_psi = convert_pressure(read_buff);
        return pressure_psi;
    }

  private:
    MPRPolicy* _policy{nullptr};

    auto convert_pressure(uint8_t* sensor_output) -> uint16_t {
        auto pressure_read_counts =
            sensor_outputf[1] << 16 | sensor_output[2] << 8 | sensor_outputf[3];
        pressure_psi =
            (pressure_read_counts * PRESSURE_RANGE_PSI) / OUTPUT_RANGE_COUNTS;
        return pressure_psi;
    }
};

}  // namespace vacuum_pressure_sensor
