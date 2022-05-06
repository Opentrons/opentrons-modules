#include "heater_policy.hpp"

#include <algorithm>

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
auto HeaterPolicy::set_power_output(double relative_power) -> bool {
    const double relative_clamped = std::clamp(relative_power, 0.0, 1.0);
    return (heater_hardware_power_set(
        hardware_handle,
        // The macro HEATER_PAD_PWM_GRANULARITY purposefully uses integer
        // division since the end goal is in fact an integer
        // NOLINTNEXTLINE(bugprone-integer-division)
        static_cast<uint16_t>(static_cast<double>(HEATER_PAD_PWM_GRANULARITY) *
                              relative_clamped)));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
auto HeaterPolicy::disable_power_output() -> void {
    heater_hardware_power_disable(hardware_handle);
}
