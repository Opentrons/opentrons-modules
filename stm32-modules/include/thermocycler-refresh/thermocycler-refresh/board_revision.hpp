/**
 * @file board_revision.hpp
 * @brief Provides an interface to check the revision number of the board that
 * firmware is running on.
 */
#pragma once

#include <array>

#include "systemwide.h"

namespace board_revision {

// Enumeration of possible board revisions
enum BoardRevision {
    BOARD_REV_1 = 1,
    BOARD_REV_2 = 2,
    BOARD_REV_INVALID = 0xFF
};

class BoardRevisionIface {
  public:
    /**
     * @brief Get the board revision. Only reads the pins if they
     * have not been read yet.
     * @return The board revision
     */
    static auto get() -> BoardRevision;
    /**
     * @brief Read the board revision
     * @return The board revision
     */
    static auto read() -> BoardRevision;
};
}  // namespace board_revision
