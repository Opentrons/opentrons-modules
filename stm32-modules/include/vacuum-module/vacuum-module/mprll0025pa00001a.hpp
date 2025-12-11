#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "firmware/pressure_policy.hpp"
#include "systemwide.h"

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
    { p.is_device_ready(dev_addr) } -> std::same_as<bool>;
};

constexpr uint16_t DEFAULT_DEV_ADDR = 0x18 << 1;
constexpr uint8_t MEASURE_PRESSURE_COMMAND = 0xAA;
constexpr uint8_t STATUS_BUSY_FLAG = 0x20;
constexpr uint32_t OUTPUT_MAX = 15099494;
constexpr uint32_t OUTPUT_MIN = 1677722;
constexpr uint32_t OUTPUT_RANGE_COUNTS = OUTPUT_MAX - OUTPUT_MIN;
// range for this particular model is 0-25 PSI
constexpr uint16_t PRESSURE_RANGE_PSI = 25;
constexpr uint8_t WRITE_LEN = 2;
constexpr uint8_t PRESSURE_FRAME_LEN = 4;
constexpr int FILTER_TAPS = 3;
constexpr int FILTERED_PRESSURE_LEN = 50;
// need to just tune these values based on testing outcomes
constexpr std::array<float, FILTER_TAPS> FILTER_VALUES = {0.6, 0.5, 0.5};
constexpr float PSI_TO_MBAR = 68.94757;
constexpr int DEFAULT_RETRIES = 5;

struct StatusByte {
    uint8_t power_indication;
    uint8_t busy_flag;
    uint8_t error_flag;
    uint8_t math_saturation;
};

template <typename Policy>
requires MPRPolicy<Policy>
class MPRLL0025PA00001 {
  public:
    MPRLL0025PA00001(uint8_t device_address)
        : _device_address{device_address} {}
    // take in device addr
    // also keep unfiltered pressure buffer
    auto initialize(Policy* policy, PressureSensorID sensor_id) -> bool {
        if (_policy == nullptr) {
            _policy = policy;
            _sensor_id = sensor_id;
        }
        return _policy->is_device_ready(_device_address << 1);
    }

    // TODO: separate sending the write pressure command,
    // and read the pressure from a callback for an eoc pin irq
    auto read_pressure() -> std::optional<double> {
        sensor_busy = true;
        _policy->i2c_master_write(_device_address << 1, WRITE_BUFF.data(),
                                  static_cast<uint16_t>(1));
        for (int i = 0; i < DEFAULT_RETRIES; i++) {
            _policy->sleep_ms(3);
            _policy->i2c_master_read(_device_address << 1, READ_BUFF.data(),
                                     PRESSURE_FRAME_LEN);
            std::memcpy(sensor_output.data(), READ_BUFF.data(),
                        PRESSURE_FRAME_LEN);
            uint8_t status_byte = sensor_output[0];
            sensor_busy = static_cast<bool>(status_byte & STATUS_BUSY_FLAG);

            if (!sensor_busy) {
                break;
            }
        }
        if (sensor_busy) {
            return std::nullopt;
        }
        double pressure_mbar = convert_pressure();
        buffer_pressure_value(pressure_mbar);
        filter_pressure();
        return pressure_mbar;
    }

  private:
    std::array<uint8_t, PRESSURE_FRAME_LEN> READ_BUFF = {0x00};
    std::array<uint8_t, PRESSURE_FRAME_LEN> sensor_output = {0x00};
    std::array<uint8_t, 1> WRITE_BUFF = {0x00};
    bool sensor_busy = true;
    // both of these act as FIFO buffers
    std::array<double, FILTER_TAPS> PRESSURE_BUFFER_MBAR = {0};
    std::array<double, FILTERED_PRESSURE_LEN> FILTERED_PRESSURE_MBAR = {0};
    int pressure_input_index = 0;
    int filtered_pressure_buffer_index = 0;
    PressureSensorID _sensor_id = PressureSensorID::ABS_PRESSURE_A;
    uint8_t _device_address;
    Policy* _policy{nullptr};

    auto get_latest_filtered_pressure() -> double {
        return FILTERED_PRESSURE_MBAR[filtered_pressure_buffer_index];
    }

    auto get_filtered_pressure_buffer() -> double* {
        return FILTERED_PRESSURE_MBAR.data();
    }

    auto filter_pressure() -> void {
        double filter_output = 0;
        int current_term = pressure_input_index;
        for (int i = 0; i < FILTER_TAPS; i++) {
            current_term -= i;
            filter_output += PRESSURE_BUFFER_MBAR.at(current_term) *
                             FILTER_VALUES.at(current_term);
        }
        // TODO: abstract this buffer processing into its own function
        FILTERED_PRESSURE_MBAR.at(filtered_pressure_buffer_index) =
            filter_output;
        filtered_pressure_buffer_index++;
        if (filtered_pressure_buffer_index == FILTERED_PRESSURE_LEN) {
            filtered_pressure_buffer_index = 0;
        }
    }

    auto buffer_pressure_value(double pressure_mbar) -> void {
        PRESSURE_BUFFER_MBAR.at(pressure_input_index) = pressure_mbar;
        pressure_input_index++;
        if (pressure_input_index == FILTER_TAPS) {
            pressure_input_index = 0;
        }
    }

    auto convert_pressure() -> double {
        auto pressure_read_counts =
            static_cast<double>(sensor_output.at(3) | sensor_output.at(2) << 8 |
                                sensor_output.at(1) << 16);
        double pressure_psi =
            ((pressure_read_counts - OUTPUT_MIN) * PRESSURE_RANGE_PSI) /
            (OUTPUT_MAX - OUTPUT_MIN);
        double pressure_mbar = pressure_psi * PSI_TO_MBAR;
        return pressure_mbar;
    }
};

};  // namespace vacuum_pressure_sensor
