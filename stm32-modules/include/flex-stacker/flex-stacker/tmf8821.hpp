#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <numbers>
#include <optional>

#include "core/bit_utils.hpp"
#include "flex-stacker/tmf8821_registers.hpp"
#include "systemwide.h"
//#include "tof_driver_policy.hpp"

namespace tmf8821 {
using namespace std::numbers;
using namespace tof::hardware;

template <typename P>
concept TMF8821Policy = requires(P p, uint16_t dev_addr , uint16_t reg, uint16_t size, uint8_t *data) {
    { p.i2c_read(dev_addr, reg, size) } -> std::same_as<RxTxReturn>;
    { p.i2c_write(dev_addr, reg, data, size) } -> std::same_as<RxTxReturn>;
};

class TMF8821 {
  public:
    template <TMF8821Policy Policy>
    auto initialize_sensor(const TMF8821RegisterMap& registers, Policy* policy,
                           TOFSensorID sensor_id) -> bool {
        return true;
    }

    template <tmf8821::TMF8821Register Reg, TMF8821Policy Policy>
    requires WritableRegister<Reg>
    auto write(Reg reg, Policy* policy, TOFSensorID sensor_id) -> bool {
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        //auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
        //value &= Reg::value_mask;
        auto value = 0;
        auto dev_address = 0x41;  // TODO: Fix
        return policy.i2c_write(dev_address, Reg::address, value);
    }

    auto read(RegisterType type, uint32_t reg, TOFSensorID sensor_id)
        -> std::optional<RegisterSerializedType> {
        using RT = std::optional<RegisterSerializedType>;
        // TODO: This should be done by some message builder
        auto size = 1;
        auto dev_address = 0x41;  // TODO: Fix
        
        static_cast<void>(size);
        static_cast<void>(dev_address);
        return RT();
        //auto ret = policy.i2c_read(dev_address, Reg::address, size);
        //if (!ret.has_value()) {
        //    return RT();
        //}
        // Ignore the typical linter warning because we're only using
        // this on __packed structures that mimic hardware registers
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        //return RT(*reinterpret_cast<Reg*>(&ret.value()));
    }

    bool _initialized = false;
};

}  // namespace tmf8821
