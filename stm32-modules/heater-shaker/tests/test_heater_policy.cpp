#include "test/test_heater_policy.hpp"

TestHeaterPolicy::TestHeaterPolicy(bool pgood, bool can_reset)
    : power_good_val(pgood), may_reset(can_reset) {}

TestHeaterPolicy::TestHeaterPolicy() : TestHeaterPolicy(false, false) {}

[[nodiscard]] auto TestHeaterPolicy::power_good() const -> bool {
    return power_good_val;
}

[[nodiscard]] auto TestHeaterPolicy::try_reset_power_good() -> bool {
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
