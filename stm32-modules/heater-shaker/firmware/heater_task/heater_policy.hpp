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
    auto set_power_output(double relative_power) -> void;
    auto disable_power_output() -> void;
    // The latch hardware requires some amount of time where the latch is held
    // low. That time isn't very long (it's ns, this is digital logic) but it is
    // non-zero, and this is how long we can delay without busy waiting
    static constexpr uint32_t HEATER_LATCH_DRIVE_DELAY_TICKS = 1;
    // Similarly, we need to wait a bit between releasing the latch line and
    // sensing the output; this also isn't very long (though it's microseconds
    // this time), but again this is our finest wait without busywaiting, so
    // that's what we use
    static constexpr uint32_t HEATER_LATCH_RELEASE_TO_SENSE_DELAY_TICKS = 1;

  private:
    heater_hardware* hardware_handle;
};
