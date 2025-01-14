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

    auto initialize_sensor(const TMF8821RegisterMap& registers,
                           TOFDriverPolicy* policy, TOFSensorID sensor_id) -> bool {

        if (_initialized) return true;
        if (!_policy) _policy = policy;

        // FOR TESTING
        return true;

        _registers = registers;
        _sensor_id = sensor_id;

        _policy->enable_tof_sensor(sensor_id, true);
        _policy->sleep_ms(20);  // sleep for 20ms for device to boot

        // Make sure the sensor is ready
        if (!set_sensor_ready(sensor_id)) return false;
        // Make sure the sensor is not in bootloader mode
        if (get_sensor_mode(sensor_id) == TOFSensorMode::BOOTLOADER) {
            if (!handle_bootloader(sensor_id)) return false;
            // Update was successful, configure the sensor
        }

        if(!configure_sensor(registers, sensor_id)) return false;

        _initialized = true;
        return _initialized;
    }

    [[nodiscard]] auto get_enable_reg() -> std::optional<tmf8821::Enable> {
        auto ret = read_register<tmf8821::Enable>(_sensor_id);
        if (ret.has_value()) {
            _registers.enable = ret.value();
        }
        return ret;
    }

    auto update_enable(const TMF8821RegisterMap& registers, TOFSensorID sensor_id) -> bool {
        if (!_initialized) return false;
        auto reg = registers.enable;
        reg.padding_1 = 0;
        return set_register(reg, sensor_id).has_value();
    }

    auto write(uint16_t reg, uint32_t* data, TOFSensorID sensor_id) -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        // TODO: validate register based on the mode
        auto dev_address = get_sensor_address(sensor_id);
        auto [res, _] = _policy->i2c_write(dev_address, reg, (uint8_t *) data, 1);
        if (res != 0) return RT();
        return RT(res);
    }

    auto read(uint32_t reg, TOFSensorID sensor_id)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        // TODO: validate register based on the mode
        auto dev_address = get_sensor_address(sensor_id);
        auto [res, data] = _policy->i2c_read(dev_address, reg, 1);
        if (res != 0) return RT();
        auto value = static_cast<uint32_t>(*data.data());
        return RT(value);
    }

    // Gets the sensor i2c address
    auto get_sensor_address(TOFSensorID sensor_id) -> uint16_t {
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
        auto ret = write(Reg::address, &value, sensor_id);
        if (!ret.has_value()) return RT();
        return RT(reg);
    }

    auto set_sensor_ready(TOFSensorID sensor_id) -> bool {
        auto ret = read_register<tmf8821::Enable>(sensor_id);
        if (!ret.has_value()) return false;

        auto reg = static_cast<tmf8821::Enable>(ret.value());
        if (!reg.pon || !reg.cpu_ready) {
            _registers.enable.pon = 1;
            _registers.enable.powerup_select = reg.powerup_select;
            update_enable(_registers, sensor_id);
            // check enable register again after 100ms
            _policy->sleep_ms(100);
            ret = read_register<tmf8821::Enable>(sensor_id);
            if (ret.has_value()) {
                auto reg = static_cast<tmf8821::Enable>(ret.value());
                // device is not ready for comms
                if (!reg.pon || !reg.cpu_ready) return false;
            }
        }
        return true;
    }

    auto get_sensor_mode(TOFSensorID sensor_id) -> TOFSensorMode {
        // check what app the sensor is running
        auto ret = read_register<tmf8821::AppID>(sensor_id);
        if (!ret.has_value()) return TOFSensorMode::UNKNOWN;
        auto appid = static_cast<tmf8821::AppID>(ret.value()).appid;
        _mode = static_cast<TOFSensorMode>(appid);
        return _mode;
    }

    auto handle_bootloader(TOFSensorID sensor_id) -> bool {
        // TODO: perform image download
        return true;
    }

    auto configure_sensor(const TMF8821RegisterMap& registers, TOFSensorID sensor_id) -> bool {
        if(!update_enable(registers, sensor_id)) return false;

        return true;
    }

    TOFDriverPolicy* _policy{nullptr};
    tmf8821::TMF8821RegisterMap _registers = {};
    bool _initialized = false;
    TOFSensorMode _mode = TOFSensorMode::UNKNOWN;
    TOFSensorID _sensor_id = TOFSensorID::NONE;
};

}  // namespace tmf8821
