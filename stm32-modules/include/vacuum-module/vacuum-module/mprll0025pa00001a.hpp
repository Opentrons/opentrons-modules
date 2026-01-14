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
constexpr int UNFILTERED_PRESSURE_BUFFER_LEN = 13;
constexpr int FILTER_LEN = 13;
constexpr int FILTERED_PRESSURE_BUFFER_LEN = 13;
double FILTER[FILTER_LEN] = {};

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

            // check device status
            ok = _policy->is_device_ready(device_address << 1);
        }
        // initialize filter buffer to be an unweighted moving average
        // TODO: optimize these filter coefficients
        double moving_average_coefficient = 1 / FILTER_LEN;
        for (int i = 0; i < FILTER_LEN; i++) {
            FILTER[i] = moving_average_coefficient;
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
                pressure_mbar = parse_pressure(RD_BUFF.data());
                filter_pressure(pressure_mbar);
                return FILTERED_PRESSURE_MBAR[filtered_pressure_buffer_index];
            }
        }

        return -1;
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

    auto filter_pressure(double input_pressure_mbar) -> void {
        filtered_pressure_buffer_index++;
        unfiltered_pressure_buffer_index++;

        filtered_pressure_buffer_index %= FILTERED_PRESSURE_BUFFER_LEN;
        unfiltered_pressure_buffer_index %= UNFILTERED_PRESSURE_BUFFER_LEN;

        UNFILTERED_PRESSURE_BUFFER_MBAR[unfiltered_pressure_buffer_index] =
            input_pressure_mbar;
        double filter_output = 0;
        for (int i = 0; i < FILTER_LEN; i++) {
            int current_term = (unfiltered_pressure_buffer_index - i) %
                               UNFILTERED_PRESSURE_BUFFER_LEN;
            filter_output +=
                UNFILTERED_PRESSURE_BUFFER_MBAR[current_term] * FILTER[i];
        }
        FILTERED_PRESSURE_MBAR[filtered_pressure_buffer_index] = filter_output;
    }

    Policy* _policy{nullptr};
    std::array<uint8_t, PRESSURE_FRAME_LEN> RD_BUFF = {0};
    std::array<uint8_t, PRESSURE_FRAME_LEN> WR_BUFF = {0};
    PressureSensorID _sensor_id{};
    uint8_t device_address{};

    double FILTERED_PRESSURE_MBAR[FILTERED_PRESSURE_BUFFER_LEN] = {0};
    double UNFILTERED_PRESSURE_BUFFER_MBAR[UNFILTERED_PRESSURE_BUFFER_LEN] = {
        0};
    int filtered_pressure_buffer_index = -1;
    int unfiltered_pressure_buffer_index = -1;
    double pressure_mbar = 0;
    uint8_t last_status = 0;
};

}  // namespace vacuum_pressure_sensor
