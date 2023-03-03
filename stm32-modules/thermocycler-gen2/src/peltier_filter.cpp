#include "thermocycler-gen2/peltier_filter.hpp"

#include <cstdlib>

using namespace peltier_filter;

auto PeltierFilter::reset() -> void { _last = 0.0F; }

[[nodiscard]] auto PeltierFilter::set_filtered(double setting, double delta_sec)
    -> double {
    const auto max_change = delta_sec * MAX_DELTA;
    if (std::abs(setting - _last) > max_change) {
        // Just change by the max change for this tick
        auto change = (setting > _last) ? max_change : -max_change;
        setting = _last + change;
    }
    _last = setting;
    return _last;
}

[[nodiscard]] auto PeltierFilter::get_last() const -> double { return _last; }