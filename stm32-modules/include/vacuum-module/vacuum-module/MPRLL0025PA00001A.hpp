#pragma once

#pragma GCC push_options
#pragma GCC optimize("O0")

#include <array>
#include <cstdint>
#include <optional>

#include "firmware/vacuum_pressure_sensor_policy.hpp"

namespace vacuum_pressure_sensor {
using i2c::hardware::RxTxReturn;

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
// std::array<uint8_t, PRESSURE_FRAME_LEN> READ_BUFF = {0};

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

    // TODO: separate sending the write pressure command,
    // and read the pressure from a callback for an eoc pin irq
    auto read_pressure() -> double {
        _policy->i2c_master_write(DEV_ADDRESS, this->WRITE_BUFF, 2);

//         _policy->i2c_write(DEV_ADDRESS, MEASURE_PRESSURE_COMMAND, READ_BUFF,
  //                         0);
        for (int i = 0; i < default_retries; i++) {
            _policy->sleep_ms(3);
            _policy->i2c_master_read(DEV_ADDRESS, this->READ_BUFF, 4);
            std::size_t size = 4;
            std::memcpy(this->sensor_output, this->READ_BUFF, size);
            uint8_t status_byte = this->sensor_output[0];
            this->sensor_busy = static_cast<bool>(status_byte & STATUS_BUSY_FLAG);

            if (!this->sensor_busy) {
                break;
            }
        }
        auto pressure_psi = this->convert_pressure();
        return pressure_psi;
    }

//    std::array<uint8_t, PRESSURE_FRAME_LEN> READ_BUFF = {0};
 //   std::array<uint8_t, PRESSURE_FRAME_LEN> WRITE = {MEASURE_PRESSURE_COMMAND};
    uint8_t READ_BUFF[4] = {0x00};
    uint8_t WRITE_BUFF[2] = {0x01, 0xAA};
    uint8_t sensor_output[4] = {0x00};
    double pressure_psi;
    double pressure_mbar;
    bool sensor_busy = true;
    int default_retries = 5;

  private:
    hardware::VacuumPressureSensorPolicy* _policy{nullptr};

    auto convert_pressure() -> uint16_t {
        auto pressure_read_counts =
            this->sensor_output[1] << 24 | this->sensor_output[2] << 16 | this->sensor_output[3] << 8;
        this->pressure_psi =
            (pressure_read_counts * PRESSURE_RANGE_PSI) / OUTPUT_RANGE_COUNTS;
        return pressure_psi;
    }
};

};  // namespace vacuum_pressure_sensor
#pragma GCC pop_options
