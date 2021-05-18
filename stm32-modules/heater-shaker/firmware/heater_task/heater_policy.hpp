#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "heater_hardware.h"
#pragma GCC diagnostic pop

class HeaterPolicy {
  public:
    HeaterPolicy() = delete;
    explicit HeaterPolicy(heater_hardware* hardware);
    [[nodiscard]] auto power_good() const -> bool;
    [[nodiscard]] auto try_reset_power_good() -> bool;

  private:
    heater_hardware* hardware_handle;
};
