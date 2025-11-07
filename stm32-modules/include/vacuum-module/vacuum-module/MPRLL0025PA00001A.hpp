#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "firmware/vacuum_pressure_sensor_policy.hpp"

namespace vacuum_pressure_sensor {

template <typename P>
concept MPRPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                             uint16_t size, uint8_t* data) {
    {
        p.i2c_read(dev_addr, reg, data, size)
        } -> std::same_as<i2c::hardware::RxTxReturn>;
    {
        p.i2c_write(dev_addr, reg, data, size)
        } -> std::same_as<i2c::hardware::RxTxReturn>;
};

constexpr uint8_t DEV_ADDRESS = 0x18 << 1;
constexpr uint8_t MEASURE_PRESSURE_COMMAND = 0xAA;
constexpr uint8_t STATUS_BUSY_FLAG = 0x20;
constexpr uint32_t OUTPUT_MAX = 15099494;
constexpr uint32_t OUTPUT_MIN = 1677722;
constexpr uint32_t OUTPUT_RANGE_COUNTS = OUTPUT_MAX - OUTPUT_MIN;
// range for this particular model is 0-25 PSI
constexpr uint16_t PRESSURE_RANGE_PSI = 25;

constexpr uint8_t PRESSURE_FRAME_LEN = 4;
std::array<uint8_t, PRESSURE_FRAME_LEN> READ_BUFF = {0};

struct StatusByte {
    uint8_t power_indication;
    uint8_t busy_flag;
    uint8_t error_flag;
    uint8_t math_saturation;
};

class MPRLL0025PA00001 {
  public:
    auto initialize(hardware::VacuumPressureSensorPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    auto read_pressure(int retries) -> uint16_t {
        bool sensor_busy = true;

        _policy->i2c_write(DEVICE_ADDRESS, MEASURE_PRESSURE_COMMAND, READ_BUFF,
                           0);

        for (int i = 0; i < retries; i++) {
            _policy->i2c_master_read(DEV_ADDRESS, READ_BUFF, 4);
            auto status_byte = READ_BUFF[0];
            sensor_busy = static_cast<bool>(status_byte & STATUS_BUSY_FLAG);

            if (!sensor_busy) {
                break;
            }
        }

        auto pressure_psi = convert_pressure(READ_BUFF);
        return pressure_psi;
    }

  private:
    hardware::VacuumPressureSensorPolicy* _policy{nullptr};

    auto convert_pressure(uint8_t* sensor_output) -> uint16_t {
        auto pressure_read_counts =
            sensor_output[1] << 16 | sensor_output[2] << 8 | sensor_output[3];
        auto pressure_psi =
            (pressure_read_counts * PRESSURE_RANGE_PSI) / OUTPUT_RANGE_COUNTS;
        return pressure_psi;
    }
};

}  // namespace vacuum_pressure_sensor
