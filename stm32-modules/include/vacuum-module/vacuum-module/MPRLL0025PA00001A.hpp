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
constexpr uint8_t WRITE_LEN = 2;
constexpr uint8_t PRESSURE_FRAME_LEN = 4;

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
    auto read_pressure() -> std::optional<double> {
        sensor_busy = true;
        _policy->i2c_master_write(DEV_ADDRESS, WRITE_BUFF, static_cast<uint16_t>(1));
        for (int i = 0; i < default_retries; i++) {
            _policy->sleep_ms(3);
            _policy->i2c_master_read(DEV_ADDRESS, READ_BUFF,
                                     PRESSURE_FRAME_LEN);
            std::memcpy(this->sensor_output, this->READ_BUFF,
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
        double pressure_psi = convert_pressure();
        return pressure_psi;
    }

    uint8_t READ_BUFF[PRESSURE_FRAME_LEN] = {0x00};
    uint8_t WRITE_BUFF[1] = {0xAA};
    uint8_t sensor_output[4] = {0x00};
    bool sensor_busy = true;
    int default_retries = 5;

  private:
    hardware::VacuumPressureSensorPolicy* _policy{nullptr};

    auto convert_pressure() -> double {
        double pressure_read_counts = static_cast<double>(sensor_output[3] |
                                    sensor_output[2] << 8 |
                                    sensor_output[1] << 16);
       double pressure_psi = ((pressure_read_counts - OUTPUT_MIN) * PRESSURE_RANGE_PSI) / (OUTPUT_MAX - OUTPUT_MIN);
        return pressure_psi; 
   }
};

};  // namespace vacuum_pressure_sensor
