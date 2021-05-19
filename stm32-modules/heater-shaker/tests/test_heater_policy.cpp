#include "test/test_heater_policy.hpp"

#include <cstddef>

TestHeaterPolicy::TestHeaterPolicy(bool pgood, bool can_reset)
    : power_good_val(pgood), may_reset(can_reset), try_reset_calls(0) {}

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
