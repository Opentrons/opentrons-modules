/**
 * @file test_board_revision_hardware.hpp
 * @brief Test-specific board revision function definitions
 */
#pragma once

#include <array>

#include "thermocycler-refresh/board_revision.hpp"

namespace board_revision {
/**
 * @brief Changes the settings of the revision input pins for test purposes.
 * @param inputs The inputs to set.
 */
auto set_pin_values(
    const std::array<TrinaryInput_t, BOARD_REV_PIN_COUNT> inputs) -> void;
}  // namespace board_revision
