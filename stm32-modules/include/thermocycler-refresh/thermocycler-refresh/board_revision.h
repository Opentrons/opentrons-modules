/**
 * @file board_revision.h
 * @brief Provides an interface to check the revision number of the board that
 * firmware is running on. This is implemented as a C function because both
 * C and C++ code may change behavior based off of the revision number
 * of the board.
 */
#ifndef BOARD_REVISION_H_
#define BOARD_REVISION_H_
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Enumeration of possible board revisions
typedef enum BoardRevision {
    BOARD_REV_1,
    BOARD_REV_2,
    BOARD_REV_INVALID
} BoardRevision_t;

/**
 * @brief Get the configured board revision
 * @return The board revision, or BOARD_REV_INVALID if the configuration
 * doesn't match a revision supported by this firmware.
 */
BoardRevision_t board_revision_get(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* BOARD_REVISION_H_ */
