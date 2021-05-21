#include "heater_policy.hpp"

#include "FreeRTOS.h"
#include "task.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "heater_hardware.h"
#pragma GCC diagnostic pop

HeaterPolicy::HeaterPolicy(heater_hardware* hardware)
    : hardware_handle(hardware) {}

// These functions have the nolints because yes, they _could_ be static; yes,
// they _could_ be const; but the semantics we want to expose here are that
// - they require internal state (they do, but it's implicit in the micro)
// - power_good inherently does not modify state but try_reset_power_good does
//   because it could change a gpio output
// But none of this is really happening in the c++ side.

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
[[nodiscard]] auto HeaterPolicy::power_good() const -> bool {
    return heater_hardware_sense_power_good();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
[[nodiscard]] auto HeaterPolicy::try_reset_power_good() -> bool {
    heater_hardware_drive_pg_latch_low();
    vTaskDelay(HEATER_LATCH_DRIVE_DELAY_TICKS);
    heater_hardware_release_pg_latch();
    vTaskDelay(HEATER_LATCH_RELEASE_TO_SENSE_DELAY_TICKS);
    return power_good();
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
auto HeaterPolicy::set_power_output(double relative_power) -> void {
    if (relative_power > 1.0 || relative_power < 0.0) {
        return;
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
auto HeaterPolicy::disable_power_output() -> void {}
