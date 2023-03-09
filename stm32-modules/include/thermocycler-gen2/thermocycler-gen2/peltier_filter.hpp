/**
 * @file peltier_filter.hpp
 * @brief Implements a simple filter on the output power of a peltier to
 * enforce a maximum ∆power/sec limit.
 */

#pragma once

#include "systemwide.h"

namespace peltier_filter {

using PowerPerSec = double;

/** Number of seconds in 100ms */
static constexpr double ONE_HUNDRED_MS = 0.1;
/**
 * Maximum rate of change is -100% to 100% in one hundred milliseconds.
 * This effectively means that changing from max cooling to max heating
 * will take 100ms.
 */
static constexpr PowerPerSec MAX_DELTA = (2.0 / ONE_HUNDRED_MS);

/**
 * Provides a simple filter on Peltier power values to ease the stress on
 * on the peltiers over their lifetime.
 */
class PeltierFilter {
  public:
    /**
     * @brief Reset the filter. This should be called whenever a peltier is
     * disabled.
     */
    auto reset() -> void;

    /**
     * @brief Set a new peltier power value and filter it based on the
     * last value that was set.
     *
     * @param setting The desired power, in the range [-1.0, 1.0]
     * @param delta_sec The number of seconds that have elapsed since the
     * last setting.
     * @return The power that should be set on the peltier.
     */
    [[nodiscard]] auto set_filtered(double setting, double delta_sec) -> double;

    /**
     * @brief Get the last filtered setting for this peltier.
     *
     * @return The most recent filtered setting
     */
    [[nodiscard]] auto get_last() const -> double;

  private:
    /** The last setting for this peltier.*/
    double _last = 0.0;
};

}  // namespace peltier_filter