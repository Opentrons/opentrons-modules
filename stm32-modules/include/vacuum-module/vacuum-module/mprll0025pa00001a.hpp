#pragma once

#include <array>
#include <cstdint>

namespace vacuum_pressure_sensor {
using i2c::hardware::RxTxReturn;

template <typename P>
concept MPRPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                             uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

constexpr uint8_t DEV_ADDRESS = 0x18;
constexpr uint8_t MEASURE_PRESSURE_COMMAND = 0xAA;
constexpr uint8_t STATUS_BUSY_FLAG = 0x20;

// output at maximum pressure [counts]
constexpr double OUTPUT_MAX = 15099494;
// output at minimum pressure [counts]
constexpr double OUTPUT_MIN = 1677722;
// maximum value of pressure range [bar, psi, kPa, etc.]
constexpr double PMAX = 25;
// minimum value of pressure range [bar, psi, kPa, etc.]
constexpr double PMIN = 0;
constexpr double PSI2MBAR = 68.9475729318;

constexpr uint8_t PRESSURE_FRAME_LEN = 10;

// Frame retry defaults
constexpr uint8_t DEFAULT_RETRIES = 3;
constexpr uint32_t DEFAULT_SLEEP_MS = 1;

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
    MPRLL0025PA00001(uint8_t dev_address) : device_address{dev_address} {}

    auto initialize(Policy* policy, PressureSensorID sensor_id) -> bool {
        if (_policy == nullptr) {
            _policy = policy;
            _sensor_id = sensor_id;
        }

        return true;
    }

    [[nodiscard]] auto get_pressure() const -> double { return pressure_mbar; }

    auto read_pressure() -> double {
        bool sensor_busy = true;
        auto len = prepare_cmd_frame(MEASURE_PRESSURE_COMMAND, nullptr, 0);
        _policy->i2c_master_write(device_address << 1, WR_BUFF.data(), len);

        for (int i = 0; i < (DEFAULT_RETRIES + 1); i++) {
            // TODO: Needs at least 3ms for measurement
            // Find better way of doing this async.
            _policy->sleep_ms(3);
            _policy->i2c_master_read(device_address << 1, RD_BUFF.data(), 4);
            auto status_byte = RD_BUFF[0];
            sensor_busy = static_cast<bool>(status_byte & STATUS_BUSY_FLAG);
            if (!sensor_busy) {
                break;
            }
        }

        if (sensor_busy) {
            // return error
        }

        pressure_mbar = parse_pressure(RD_BUFF.data());
        return pressure_mbar;
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
        // Calculation of pressure value according to equation 2 of datasheet
        // Equation 2: Pressure Output Function
        auto press_counts =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            (double)((int32_t)raw[3] + (int32_t)raw[2] * (int32_t)256 +
                     (int32_t)raw[1] *
                         (int32_t)65536);  // calculate digital pressure counts
        auto pressure = (((press_counts - OUTPUT_MIN) * (PMAX - PMIN)) /
                         (OUTPUT_MAX - OUTPUT_MIN)) +
                        PMIN;
        return pressure * PSI2MBAR;
    }

    Policy* _policy{nullptr};
    std::array<uint8_t, PRESSURE_FRAME_LEN> RD_BUFF = {0};
    std::array<uint8_t, PRESSURE_FRAME_LEN> WR_BUFF = {0};
    PressureSensorID _sensor_id;
    uint8_t device_address;

    double pressure_mbar = {0};
};

}  // namespace vacuum_pressure_sensor
