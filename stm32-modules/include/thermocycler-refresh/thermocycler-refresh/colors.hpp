/**
 * @file colors.hpp
 * @brief Provide enumeration of colors on the system
 */

#pragma once

#include "core/xt1511.hpp"

namespace colors {

// Enumerated colors to simplify controlling the system UI
enum class Colors { SOFT_WHITE, WHITE, RED, GREEN, BLUE, ORANGE, NONE };

// Enumerated actions for the system UI
enum class Mode {
    SOLID,    /**< Standard mode - just hold color steady.*/
    PULSING,  /**< Pulse the selected color from 0 to 100% brightness.*/
    BLINKING, /**< Blink the light on and off.*/
    WIPE,     /**< Wipe across the UI left to right.*/
};

/**
 * @brief Convert an enumerated color code to an XT1511 structure
 * @param color The color code to convert
 * @param brightness The brightness value to set for this color. The
 * value for each LED will be linearly scaled by this amount, on a
 * scale of 0 to 1.0
 * @return xt1511::XT1511  containing the corresponding color mapped to
 * the correct brightness
 */
auto get_color(Colors color, double brightness = 1.0F) -> xt1511::XT1511;

}  // namespace colors
