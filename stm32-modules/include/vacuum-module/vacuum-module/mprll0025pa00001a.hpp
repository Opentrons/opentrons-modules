#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "firmware/hardware_iface.hpp"

namespace vacuum_pressure_sensor {
using i2c::hardware::RxTxReturn;

template <typename P>
concept MPRPolicy = requires(P p, uint16_t dev_addr, uint16_t reg,
                             uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.is_device_ready(dev_addr) } -> std::same_as<bool>;
};

constexpr uint8_t DEV_ADDRESS = 0x18;
constexpr uint8_t MEASURE_PRESSURE_COMMAND = 0xAA;
constexpr std::array<uint8_t, 2> MEASURE_PRESSURE_COMMAND_DATA = {0};

// Bit 5 (Busy flag): Indicates that the data for the last command is not yet
// available. No new commands are processed if the device is busy.
constexpr uint8_t STATUS_BUSY_FLAG = 0x20;
// Bit 2 (Memory integrity/error flag): Indicates whether the checksum-based
// integrity check passed or failed; the memory error status bit is calculated
// only during the power-up sequence.
constexpr uint8_t STATUS_ERROR_FLAG = 0x2;
// Bit 0 (Math saturation): 1 = internal math saturation has occurred
constexpr uint8_t STATUS_SATURATION_FLAG = 0x1;

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
constexpr int PRESSURE_BUFFER_LEN = 8;
// These coefficients act as an EMI (Exponential Moving Average) filter.
// Ci = alpha(1 - alpha)^i
// clang-format off
const std::array<double, PRESSURE_BUFFER_LEN> FILTER = {
    // 0.1756, 0.1580, 0.1422, 0.1280, 0.1152, 0.1037, 0.0933, 0.0840  // a=0.1
    0.2403, 0.1923, 0.1538, 0.1231, 0.0985, 0.0788, 0.0630, 0.0504  // a=0.2
    // 0.3133, 0.2193, 0.1535, 0.1075, 0.0752, 0.0527, 0.0369, 0.0258  // a=0.3
    // 0.5020, 0.2510, 0.1255, 0.0627, 0.0314, 0.0157, 0.0078, 0.0039  // a=0.5
    // 0.8000, 0.1600, 0.0320, 0.0064, 0.0013, 0.0003, 0.0001, 0.0000  // a=0.8
};
// clang-format on

// Frame retry defaults
constexpr uint8_t DEFAULT_RETRIES = 3;
constexpr uint32_t DEFAULT_SLEEP_MS = 10;

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
        auto ok = false;
        if (_policy == nullptr) {
            _policy = policy;
            _sensor_id = sensor_id;
            is_first_read = true;

            // check device status
            ok = _policy->is_device_ready(device_address << 1);
        }

        return ok;
    }

    [[nodiscard]] auto get_pressure() const -> double { return pressure_mbar; }

    auto read_pressure() -> double {
        auto len = prepare_cmd_frame(MEASURE_PRESSURE_COMMAND,
                                     MEASURE_PRESSURE_COMMAND_DATA.data(), 2);
        _policy->i2c_master_write(device_address << 1, WR_BUFF.data(), len);

        for (int i = 0; i < (DEFAULT_RETRIES + 1); i++) {
            // TODO: Needs at least n ms for measurement
            // Find better way of doing this async.
            _policy->sleep_ms(DEFAULT_SLEEP_MS);
            _policy->i2c_master_read(device_address << 1, RD_BUFF.data(), 4);
            auto status_byte = RD_BUFF[0];

            // return negative if sensor is in error state
            if (static_cast<bool>(status_byte & STATUS_ERROR_FLAG) ||
                static_cast<bool>(status_byte & STATUS_SATURATION_FLAG)) {
                return -1;
            }
            auto sensor_busy =
                static_cast<bool>(status_byte & STATUS_BUSY_FLAG);
            if (!sensor_busy) {
                auto raw_pressure = parse_pressure(RD_BUFF.data());
                // Pre-fill the buffer if this is the first read so we dont
                // calculate the filtered pressure from 1 reading where the
                // rest of the buffer is holding 0.
                if (is_first_read) {
                    unfiltered_pressure_mbar.fill(raw_pressure);
                    is_first_read = false;
                }
                pressure_mbar = filter_pressure(raw_pressure);
                return pressure_mbar;
            }
        }

        return -1;
    }

    auto solid_state_target_pressure(double target_pressure, int num_samples,
                                     double tolerance) -> bool {
        int p_index = 0;
        for (int i = 0; i < num_samples; i++) {
            p_index =
                (filtered_pressure_buffer_index + i) % PRESSURE_BUFFER_LEN;
            double pressure_sample = filtered_pressure_mbar.at(p_index);
            if (std::abs(pressure_sample - target_pressure) > tolerance) {
                return false;
            }
        }
        return true;
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
        // The pressure counts are in big-endian (most significant byte first)
        // order, not little-endian.
        auto press_counts =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            static_cast<double>(raw[1] << 16 | raw[2] << 8 | raw[3]);
        auto pressure_psi = (((press_counts - OUTPUT_MIN) * (PMAX - PMIN)) /
                             (OUTPUT_MAX - OUTPUT_MIN)) +
                            PMIN;
        return pressure_psi * PSI2MBAR;
    }

    auto filter_pressure(double pressure_mbar) -> double {
        double filter_output = 0;
        unfiltered_pressure_mbar.at(pressure_buffer_index) = pressure_mbar;
        for (int i = 0; i < PRESSURE_BUFFER_LEN; i++) {
            const int p_index =
                (pressure_buffer_index - i + PRESSURE_BUFFER_LEN) %
                PRESSURE_BUFFER_LEN;
            filter_output +=
                unfiltered_pressure_mbar.at(p_index) * FILTER.at(i);
        }
        pressure_buffer_index =
            (pressure_buffer_index + 1) % PRESSURE_BUFFER_LEN;
        return filter_output;
    }

    Policy* _policy{nullptr};
    std::array<uint8_t, PRESSURE_FRAME_LEN> RD_BUFF = {0};
    std::array<uint8_t, PRESSURE_FRAME_LEN> WR_BUFF = {0};
    PressureSensorID _sensor_id{};
    uint8_t device_address{};

    std::array<double, PRESSURE_BUFFER_LEN> unfiltered_pressure_mbar = {0};
    uint8_t pressure_buffer_index = 0;
    double pressure_mbar = 0;
    uint8_t last_status = 0;
    bool is_first_read = true;
};

}  // namespace vacuum_pressure_sensor
