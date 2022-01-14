/**
 * @file colors.cpp
 * @brief Implements fetching color structures from codes
 */

#include "thermocycler-refresh/colors.hpp"

using namespace colors;

static constexpr uint8_t FULL = 0xFF;
static constexpr uint8_t HIGH = 0xEE;
static constexpr uint8_t MED = 0x50;

// Public function implementation
auto colors::get_color(Colors color, double brightness) -> xt1511::XT1511 {
    if (brightness > 1.0F) {
        brightness = 1.0F;
    }
    xt1511::XT1511 led{};
    switch (color) {
        case Colors::SOFT_WHITE:
            led.w = HIGH;
            break;
        case Colors::WHITE:
            led.g = HIGH;
            led.r = HIGH;
            led.b = HIGH;
            break;
        case Colors::RED:
            led.r = MED;
            break;
        case Colors::GREEN:
            led.g = HIGH;
            break;
        case Colors::BLUE:
            led.b = FULL;
            break;
        case Colors::ORANGE:
            led.g = MED;
            led.r = FULL;
            break;
        case Colors::NONE:
            break;
    }
    led.set_scale(brightness);
    return led;
}
