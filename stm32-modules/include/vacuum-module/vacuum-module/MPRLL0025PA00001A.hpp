#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "firmware/vacuum_pressure_sensor_policy.hpp"

namespace vacuum_pressure_sensor {
using hardware::VacuumPressureSensorPolicy;
using i2c::hardware::RxTxReturn;

template <typename P>
concept MPRPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                             uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

constexpr uint8_t DEV_ADDRESS = 0x18 << 1;
constexpr uint8_t MEASURE_PRESSURE_COMMAND = 0xAA;
constexpr uint8_t STATUS_BUSY_FLAG = 0x20;
// max sensor output in counts
constexpr uint32_t OUTPUT_MAX = 15099494;
// min sensor output in counts
constexpr uint32_t OUTPUT_MIN = 1677722;
constexpr uint32_t OUTPUT_RANGE_COUNTS = OUTPUT_MAX - OUTPUT_MIN;
// range for this particular model is 0-25 PSI
constexpr uint16_t PRESSURE_RANGE_PSI = 25;
// 1 PSI = 68.9476 mbar
constexpr uint32_t MBAR_CONVERSION_FACTOR = 68.9476;
constexpr uint8_t PRESSURE_FRAME_LEN = 10;
constexpr int DEFAULT_RETRIES = 3;

struct StatusByte {
    uint8_t power_indication;
    uint8_t busy_flag;
    uint8_t error_flag;
    uint8_t math_saturation;
};

class MPRLL0025PA00001 {
  public:
    auto initialize(VacuumPressureSensorPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    // TODO: separate sending the write pressure command,
    // and read the pressure from a callback for an eoc pin irq
    auto read_pressure() -> std::optional<double> {
        _policy->i2c_master_write(DEVICE_ADDRESS, WRITE_BUFF, 1);

        for (int i = 0; i < (DEFAULT_RETRIES + 1); i++) {
            _policy->sleep_ms(3);
//        _policy->i2c_write(DEV_ADDRESS, MEASURE_PRESSURE_COMMAND, READ_BUFF,
//                           0);
            _policy->i2c_master_read(DEV_ADDRESS, READ_BUFF, 4);
            auto status_byte = READ_BUFF[0];
            sensor_busy = static_cast<bool>(status_byte & STATUS_BUSY_FLAG);

            if (!sensor_busy) {
                break;
            }
        }

        pressure_psi = convert_pressure(READ_BUFF);
        pressure_mbar = pressure_psi * MBAR_CONVERSION_FACTOR;
        return pressure_psi;
    }

//    std::array<uint8_t, PRESSURE_FRAME_LEN> READ_BUFF = {0};
 //   std::array<uint8_t, PRESSURE_FRAME_LEN> WRITE = {MEASURE_PRESSURE_COMMAND};
    uint8_t READ_BUFF[4] = {0x00};
    uint8_t WRITE_BUFF[1] = {MEASURE_PRESSURE_COMMAND};
    double pressure_psi;
    double pressure_mbar;
    bool sensor_busy = true;
    int default_retries = 5;

  private:
    hardware::VacuumPressureSensorPolicy* _policy{nullptr};

  private:
    VacuumPressureSensorPolicy* _policy{nullptr};

    auto convert_pressure(uint8_t* sensor_output) -> double {
        // auto pressure_read_counts =
        //     sensor_output[1] << 16 | sensor_output[2] << 8 |
        //     sensor_output[3];
        uint32_t pressure_read_counts = sensor_output[1] << 24 |
                                        sensor_output[2] << 16 |
                                        sensor_output[3] << 8;
        double pressure_psi = static_cast<double>(
            (pressure_read_counts * PRESSURE_RANGE_PSI) / OUTPUT_RANGE_COUNTS);
        return pressure_psi;
    }
};

};  // namespace vacuum_pressure_sensor
