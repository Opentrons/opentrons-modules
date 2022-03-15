#include "heater_policy.hpp"

#include <stddef.h>

#include <algorithm>
#include <cstring>

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
    const double relative_clamped = std::clamp(relative_power, 0.0, 1.0);
    heater_hardware_power_set(
        hardware_handle,
        // The macro HEATER_PAD_PWM_GRANULARITY purposefully uses integer
        // division since the end goal is in fact an integer
        // NOLINTNEXTLINE(bugprone-integer-division)
        static_cast<uint16_t>(static_cast<double>(HEATER_PAD_PWM_GRANULARITY) *
                              relative_clamped));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static,readability-make-member-function-const)
auto HeaterPolicy::disable_power_output() -> void {
    heater_hardware_power_disable(hardware_handle);
}

auto HeaterPolicy::set_thermal_offsets(flash::OffsetConstants* constants)
    -> bool {
    // convert constants to writable_offsets
    writable_offsets to_send;
    memcpy(&to_send.const_b, &constants->b, sizeof(constants->b));
    memcpy(&to_send.const_c, &constants->c, sizeof(constants->c));
    memcpy(&to_send.const_flag, &constants->flag, sizeof(constants->flag));

    if (!heater_hardware_set_offsets(&to_send)) {
        return false;
    } else {
        return true;
    }
}

auto HeaterPolicy::get_thermal_offsets() -> flash::OffsetConstants {
    // convert writable_offsets to OffsetConstants
    writable_offsets to_receive;
    to_receive.const_b =
        heater_hardware_get_offset(offsetof(struct writable_offsets, const_b));
    to_receive.const_c =
        heater_hardware_get_offset(offsetof(struct writable_offsets, const_c));
    to_receive.const_flag = heater_hardware_get_offset(
        offsetof(struct writable_offsets, const_flag));
    flash::OffsetConstants to_pass;
    memcpy(&to_pass.b, &to_receive.const_b, sizeof(to_receive.const_b));
    memcpy(&to_pass.c, &to_receive.const_c, sizeof(to_receive.const_c));
    memcpy(&to_pass.flag, &to_receive.const_flag,
           sizeof(to_receive.const_flag));
    return to_pass;
}
