/**
 * @file sim_board_revision_hardware.c
 * @brief Provides an implementation of the board_revision_get function
 * for Simulator targets
 *
 */

#include "thermocycler-gen2/board_revision_hardware.h"

extern "C" {
void board_revision_read_inputs(TrinaryInput_t *inputs) {
    if (inputs) {
        // Hard coded for a rev 1 board
        inputs[0] = INPUT_FLOATING;
        inputs[1] = INPUT_FLOATING;
        inputs[2] = INPUT_FLOATING;
    }
}
}
