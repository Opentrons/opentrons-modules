/**
* @file tmc2160.hpp
* @brief Interface to control a TMC2160 IC
*/
#pragma once

#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <numbers>
#include <optional>

#include "core/bit_utils.hpp"
#include "tmc2160_registers.hpp"
#include "tmc2160_interface.hpp"


namespace tmc2160 {

using namespace std::numbers;

template <typename Policy>
concept TMC2160Policy = TMC2160InterfacePolicy<Policy> && requires(Policy& p, uint32_t addr, uint32_t data, MotorID motor_id) {
    { p.write(addr, data, motor_id) } -> std::same_as<bool>;
    { p.read(addr, motor_id) } -> std::same_as<std::optional<uint32_t>>;
};

class TMC2160 {
 public:
   template <TMC2160Policy Policy>
   auto initialize_config(TMC2160RegisterMap& registers, Policy& policy, MotorID motor_id) -> bool {
       if (!set_registers(verify_gconf(registers.gconfig), policy, motor_id)) {
           return false;
       }
       if (!set_registers(verify_shortconf(registers.short_conf), policy, motor_id)) {
           return false;
       }
       if (!set_registers(verify_drvconf(registers.drvconf), policy, motor_id)) {
           return false;
       }
       if (!set_registers(verify_glob_scaler(registers.glob_scale), policy, motor_id)) {
          return false;
       }
       if (!set_register(verify_ihold_irun(registers.ihold_irun), policy, motor_id)) {
           return false;
       }
       if (!set_register(verify_tpowerdown(
                 PowerDownDelay::reg_to_seconds(registers.tpowerdown.time)), policy, motor_id)) {
           return false;
       }
       if (!set_register(registers.tpwmthrs, policy, motor_id)) {
           return false;
       }
       if (!set_register(registers.tcoolthrs, policy, motor_id)) {
           return false;
       }
       if (!set_register(registers.thigh, policy, motor_id)) {
           return false;
       }
       if (!set_register(verify_chopconf(registers.chopconf), policy, motor_id)) {
           return false;
       }
       if (!set_register(verify_coolconf(registers.coolconf), policy, motor_id)) {
          return false;
       }
       if (!set_register(verify_pwmconf(registers.pwmconf), policy, motor_id)) {
          return false;
       }
   }

   auto verify_gconf(GConfig& reg) -> GConfig {
       reg.test_mode = 0;
       return reg;
   }

   auto verify_shortconf(ShortConf& reg) -> ShortConf {
       reg.bit_padding_1 = 0;
       reg.bit_padding_2 = 0;
       return reg;
   }

   auto verify_drvconf(DriverConf& reg) -> DriverConf {
       reg.bit_padding_1 = 0;
       reg.bit_padding_2 = 0;
       return reg;
   }

   auto verify_ihold_irun(CurrentControl& reg) -> CurrentControl {
       reg.bit_padding_1 = 0;
       reg.bit_padding_2 = 0;
       return reg;
   }

   auto verify_tpowerdown(double time) -> PowerDownDelay {
       PowerDownDelay temp_reg = {.time =
                                      PowerDownDelay::seconds_to_reg(time)};
       return temp_reg;
   }

   auto verify_chopconf(ChopConfig& reg) -> ChopConfig {
       reg.padding_1 = 0;
       reg.padding_2 = 0;
       return reg;
   }

   auto verify_coolconf(CoolConfig& reg) -> CoolConfig {
       // Assert that bits that MUST be 0 are actually 0
       reg.padding_1 = 0;
       reg.padding_2 = 0;
       reg.padding_3 = 0;
       reg.padding_4 = 0;
       reg.padding_5 = 0;
       return reg;
   }

   auto verify_pwmconf(StealthChop& reg) -> StealthChop {
       reg.padding_0 = 0;
       return reg;
   }

   auto verify_glob_scaler(GlobalScaler& reg) -> GlobalScaler {
       reg.clamp_value();
       return reg;
   }

 private:
   /**
    * @brief Set a register on the TMC2160
    *
    * @tparam Reg The type of register to set
    * @tparam Policy Abstraction class for actual writing
    * @param[in] policy Instance of the abstraction policy to use
    * @param[in] reg The register to write
    * @return True if the register could be written, false otherwise.
    * Attempts to write to an unwirteable register will throw a static
    * assertion.
    */
   template <TMC2160Register Reg, TMC2160Policy Policy>
   requires WritableRegister<Reg>
   auto set_register(Reg& reg, Policy& policy, MotorID motor_id) -> bool {
       // Ignore the typical linter warning because we're only using
       // this on __packed structures that mimic hardware registers
       // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
       auto value = *reinterpret_cast<RegisterSerializedTypeA*>(&reg);
       value &= Reg::value_mask;
       return policy.write(Reg::address, value, motor_id);
   }
   /**
    * @brief Read a register on the TMC2160
    *
    * @tparam Reg The type of register to read
    * @tparam Policy Abstraction class for actual writing
    * @param[in] policy Instance of the abstraction policy to use
    * @return The contents of the register, or nothing if the register
    * can't be read.
    */
   template <TMC2160Register Reg, TMC2160Policy Policy>
   requires ReadableRegister<Reg>
   auto read_register(uint32_t data, Policy& policy, MotorID motor_id) -> std::optional<Reg> {
       using RT = std::optional<Reg>;
       auto ret = policy.read(Reg::address, motor_id);
       if (!ret.has_value()) {
           return RT();
       }
       // Ignore the typical linter warning because we're only using
       // this on __packed structures that mimic hardware registers
       // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
       return RT(*reinterpret_cast<Reg*>(&ret.value()));
   }
};

}  // namespace tmc2160

