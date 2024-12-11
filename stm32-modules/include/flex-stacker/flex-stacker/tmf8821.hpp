#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <numbers>
#include <optional>

#include "flex-stacker/tmf8821_registers.hpp"
#include "systemwide.h"

namespace tmf8821 {
using namespace std::numbers;
using namespace tof::hardware;

template <typename P>
concept TMF8821Policy = requires(P p, uint16_t dev_addr , uint16_t reg, uint16_t size, uint8_t *data) {
    { p.i2c_read(dev_addr, reg, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

constexpr uint16_t TOF_DEFAULT_ADDRESS = 0x41 << 1;
constexpr uint16_t TOF_X_ADDRESS = 0x39 << 1;
constexpr uint16_t TOF_Z_ADDRESS = 0x40 << 1;

class TMF8821 {
  public:
    TMF8821(TOFDriverPolicy* policy) : _policy(policy) {}
    TMF8821(const TMF8821& c) = delete;
    TMF8821(const TMF8821&& c) = delete;
    auto operator=(const TMF8821& c) = delete;
    auto operator=(const TMF8821&& c) = delete;
    ~TMF8821() = default;

    auto initialize_sensor(const TMF8821RegisterMap& registers,
                           TOFDriverPolicy* policy, TOFSensorID sensor_id) -> bool {
        if (!_policy) _policy = policy;

        // Check mode
        // Do fw update Aif required

        if(!configure_sensor(registers, sensor_id)) return false;

        _initialized = _tof_x_init && _tof_z_init;
        return true;
    }

    auto update_enable(const TMF8821RegisterMap& registers, TOFSensorID sensor_id) -> bool {
        if (!_initialized) return false;
        auto reg = registers.enable;
        reg.padding_1 = 0;
        return set_register(reg, sensor_id).has_value();
    }

    auto write(RegisterType type, uint16_t reg, uint32_t* data, TOFSensorID sensor_id) -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        auto dev_address = get_sensor_address(sensor_id);
        // TODO: (uint8_t *) should be uint32_t 
        auto [res, _] = _policy->i2c_write(dev_address, reg, (uint8_t *) data, 1);
        if (res != 0) return RT();
        return RT(res);
    }

    auto read(RegisterType type, uint32_t reg, TOFSensorID sensor_id)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        auto dev_address = get_sensor_address(sensor_id);
        auto [res, data] = _policy->i2c_read(dev_address, reg, 1);
        if (res != 0) return RT();
        auto value = static_cast<uint32_t>(*data.data());
        return RT(value);
    }

    // Gets the sensor i2c address
    auto get_sensor_address(TOFSensorID sensor_id) -> uint16_t {
        // TODO: This probably needs account for the individual sensors
        if (!_initialized) return TOF_DEFAULT_ADDRESS;
        if (sensor_id == TOF_X) return TOF_X_ADDRESS;
        if (sensor_id == TOF_Z) return TOF_Z_ADDRESS;
        return TOF_DEFAULT_ADDRESS;
    }

  private:

    template <tmf8821::TMF8821Register Reg>
    requires ReadableRegister<Reg>
    auto read_register(TOFSensorID sensor_id) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        auto type = Reg::type;  // TODO: use type based on mode
        auto ret = read(Reg::address, sensor_id);
        if (!ret.has_value()) return RT();
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return RT(*reinterpret_cast<Reg*>(&ret.value()));
    }

    template <tmf8821::TMF8821Register Reg>
    requires WritableRegister<Reg>
    auto set_register(Reg reg, TOFSensorID sensor_id) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        // TODO: Verify the Reg::mode (RegisterType)

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        value &= Reg::value_mask;
        auto ret = write(Reg::mode, Reg::address, &value, sensor_id);
        if (!ret.has_value()) return RT();
        return RT(reg);
    }

    auto configure_sensor(const TMF8821RegisterMap& registers, TOFSensorID sensor_id) -> bool {
        if(!update_enable(registers, sensor_id)) return false;

        if(sensor_id == TOF_X) _tof_x_init = true;
        if(sensor_id == TOF_Z) _tof_z_init = true;
        return true;
    }

    TOFDriverPolicy* _policy{nullptr};
    bool _initialized = false;
    bool _tof_x_init = false;
    bool _tof_z_init = false;
};

}  // namespace tmf8821
