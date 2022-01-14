/**
 * @file colors.cpp
 * @brief Implements fetching color structures from codes
 */

#include "thermocycler-refresh/colors.hpp"

using namespace colors;

namespace color_values {

static constexpr auto SOFT_WHITE = xt1511::XT1511{.w = 0xEE};
static constexpr auto WHITE = xt1511::XT1511{.g = 0xEE, .r = 0xEE, .b = 0xEE};
static constexpr auto RED = xt1511::XT1511{.r = 0x50};
static constexpr auto GREEN = xt1511::XT1511{.g = 0xEE};
static constexpr auto BLUE = xt1511::XT1511{.b = 0xFF};
static constexpr auto ORANGE = xt1511::XT1511{.g = 0x53, .r = 0xFF};
static constexpr auto NONE = xt1511::XT1511{};

}  // namespace color_values

// Public function implementation
auto colors::get_color(Colors color, double brightness) -> xt1511::XT1511 {
    if (brightness > 1.0F) {
        brightness = 1.0F;
    }
    xt1511::XT1511 led{};
    switch (color) {
        case Colors::SOFT_WHITE:
            led = color_values::SOFT_WHITE;
            break;
        case Colors::WHITE:
            led = color_values::WHITE;
            break;
        case Colors::RED:
            led = color_values::RED;
            break;
        case Colors::GREEN:
            led = color_values::GREEN;
            break;
        case Colors::BLUE:
            led = color_values::BLUE;
            break;
        case Colors::ORANGE:
            led = color_values::ORANGE;
            break;
        case Colors::NONE:
            led = color_values::NONE;
            break;
    }
    led.set_scale(brightness);
    return led;
}
