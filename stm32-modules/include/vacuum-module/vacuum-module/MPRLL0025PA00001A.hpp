#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace mpr_pressure_sensor {

template <typename P>
concept TMF8820Policy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

constexpr uint16_t WRITE_TO_SENSOR = 0x30;
constexpr uint8_t READ_FROM_SENSOR_COMMAND = 0x31;
constexpr uint16_t MEASURE_PRESSURE_BYTE = 0xAA;
// measure pressure = 30 AA 00 00
constexpr uint32_t MEASURE_PRESSURE_COMMAND =
    (WRITE_TO_SENSOR << 24) | (MEASURE_PRESSURE_BYTE << 16);

// communication loop is:
// 1. send measure pressure command, 4 bytes
// 2. send read from sensor command, 1 byte
//      3. sensor sends status byte, 1 byte
// 4. send read from pressure command, 1 byte
//      5. sensor sends pressure reading, 3 bytes

struct StatusByte {
    uint8_t power_indication;
    uint8_t busy_flag;
    uint8_t error_flag;
    uint8_t math_saturation;
};

class MPRLL0025PA00001 {
  public:
    auto initialize(MPRPressureSensorPolicy* policy) -> void {
        if (_policy == nullptr) {
            _policy = policy;
        }
    }

    // read pressure

    // read eoc pin

    // write start read command

    // function for reading and streaming data in a loop

  private:
    MPRPressureSensorPolicy* _policy{nullptr};
    std::array<uint8_t, BUFFER_LEN> BUFFER{};
};

}  // namespace mpr_pressure_sensor
