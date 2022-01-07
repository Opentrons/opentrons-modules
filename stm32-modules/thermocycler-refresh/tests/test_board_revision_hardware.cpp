/**
 * @file test_board_revision_hardware.c
 * @brief Provides an implementation of the board_revision_get function
 * for Test targets
 *
 */

#include "test/test_board_revision_hardware.hpp"

#include "thermocycler-refresh/board_revision_hardware.h"

static std::array<TrinaryInput_t, BOARD_REV_PIN_COUNT> _inputs = {
    INPUT_FLOATING, INPUT_FLOATING, INPUT_FLOATING};

extern "C" {
void board_revision_read_inputs(TrinaryInput_t *inputs) {
    if (inputs) {
        // Hard coded for a rev 1 board
        inputs[0] = _inputs.at(0);
        inputs[1] = _inputs.at(1);
        inputs[2] = _inputs.at(2);
    }
}
}

auto board_revision::set_pin_values(
    const std::array<TrinaryInput_t, BOARD_REV_PIN_COUNT> inputs) -> void {
    _inputs = inputs;
}
