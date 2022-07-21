#include "test/test_heater_policy.hpp"

#include <cstddef>

#include "systemwide.h"

TestHeaterPolicy::TestHeaterPolicy(bool pgood, bool can_reset)
    : power_good_val(pgood),
      may_reset(can_reset),
      try_reset_calls(0),
      power(0),
      enabled(false),
      circuit_error(false) {}

TestHeaterPolicy::TestHeaterPolicy() : TestHeaterPolicy(true, true) {}

[[nodiscard]] auto TestHeaterPolicy::power_good() const -> bool {
    return power_good_val;
}

[[nodiscard]] auto TestHeaterPolicy::try_reset_power_good() -> bool {
    try_reset_calls++;
    if (may_reset) {
        power_good_val = true;
    }
    return power_good_val;
}

auto TestHeaterPolicy::set_power_good(bool pgood) -> void {
    power_good_val = pgood;
}

auto TestHeaterPolicy::set_can_reset(bool can_reset) -> void {
    may_reset = can_reset;
}

auto TestHeaterPolicy::try_reset_call_count() const -> size_t {
    return try_reset_calls;
}

auto TestHeaterPolicy::reset_try_reset_call_count() -> void {
    try_reset_calls = 0;
}

auto TestHeaterPolicy::set_power_output(double output)
    -> HEATPAD_CIRCUIT_ERROR {
    power = output;
    if (!circuit_error) {
        enabled = true;
        return HEATPAD_CIRCUIT_ERROR::NONE;
    } else {
        enabled = false;
        return HEATPAD_CIRCUIT_ERROR::SHORT;
    }
}

auto TestHeaterPolicy::set_circuit_error(bool set) -> void {
    if (set) {
        circuit_error = true;
    } else {
        circuit_error = false;
    }
}

auto TestHeaterPolicy::disable_power_output() -> void { enabled = false; }

auto TestHeaterPolicy::last_power_setting() const -> double { return power; }

auto TestHeaterPolicy::last_enable_setting() const -> bool { return enabled; }

auto TestHeaterPolicy::set_thermal_offsets(flash::OffsetConstants* constants)
    -> bool {
    stored_offsets = *constants;
    return true;
}

auto TestHeaterPolicy::get_thermal_offsets() -> flash::OffsetConstants {
    return stored_offsets;
}
