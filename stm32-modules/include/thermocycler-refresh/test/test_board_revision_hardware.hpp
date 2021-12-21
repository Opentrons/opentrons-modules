/**
 * @file test_board_revision_hardware.hpp
 * @brief Test-specific board revision function definitions
 */
#pragma once

#include <array>

#include "thermocycler-refresh/board_revision.hpp"

namespace board_revision {
auto set_pin_values(
    const std::array<TrinaryInput_t, BOARD_REV_PIN_COUNT> inputs) -> void;
}
