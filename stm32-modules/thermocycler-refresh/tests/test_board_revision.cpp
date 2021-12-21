/**
 * @file test_board_revision.c
 * @brief Provides an implementation of the board_revision_get function
 * for Test targets
 * 
 */

#include "thermocycler-refresh/board_revision.h"

#define BOARD_REVISION (BOARD_REV_1)

BoardRevision_t board_revision_get(void) {
    return BOARD_REVISION;
}
