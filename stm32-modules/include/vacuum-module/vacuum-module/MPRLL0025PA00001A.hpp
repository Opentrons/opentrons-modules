#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace vacuum_pressure_sensor {

template <typename P>
concept TMF8820Policy = requires(P p, uint16_t dev_addr, uint16_t reg,
                                 uint16_t size, uint8_t* data) {
    { p.i2c_read(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

constexpr uint8_t DEV_ADDRESS = 0x18;
constexpr uint8_t MEASURE_PRESSURE_BYTE = 0xAA;

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

  private:
    VacuumressureSensorPolicy* _policy{nullptr};
    std::array<uint8_t, BUFFER_LEN> BUFFER{};
};

}  // namespace vacuum_pressure_sensor
