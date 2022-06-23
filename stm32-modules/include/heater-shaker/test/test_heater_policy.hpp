#pragma once
#include <cstddef>

#include "heater-shaker/flash.hpp"

class TestHeaterPolicy {
  public:
    TestHeaterPolicy();
    explicit TestHeaterPolicy(bool pgood, bool can_reset);
    [[nodiscard]] auto power_good() const -> bool;
    [[nodiscard]] auto try_reset_power_good() -> bool;
    auto set_power_output(double output) -> bool;
    auto disable_power_output() -> void;

    auto set_power_good(bool pgood) -> void;
    auto set_can_reset(bool can_reset) -> void;

    auto last_power_setting() const -> double;
    auto last_enable_setting() const -> bool;

    auto try_reset_call_count() const -> size_t;
    auto reset_try_reset_call_count() -> void;
    auto set_thermal_offsets(flash::OffsetConstants* constants) -> bool;
    auto get_thermal_offsets() -> flash::OffsetConstants;

  private:
    bool power_good_val;
    bool may_reset;
    size_t try_reset_calls;
    double power;
    bool enabled;
    flash::OffsetConstants stored_offsets = {};
};
