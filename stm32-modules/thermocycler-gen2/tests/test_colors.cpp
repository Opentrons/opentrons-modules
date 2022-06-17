#include "catch2/catch.hpp"
#include "thermocycler-gen2/colors.hpp"

using namespace colors;

SCENARIO("color converter works") {
    GIVEN("some colors and full brightness") {
        auto white = get_color(Colors::SOFT_WHITE);
        auto blue = get_color(Colors::BLUE);
        auto red = get_color(Colors::RED);
        THEN("the result is as expected") {
            REQUIRE(white == xt1511::XT1511{.w = 0xEE});
            REQUIRE(blue == xt1511::XT1511{.b = 0xFF});
            REQUIRE(red == xt1511::XT1511{.r = 0x50});
        }
        AND_GIVEN("some colors set to 150% brightness") {
            auto white2 = get_color(Colors::SOFT_WHITE, 1.5);
            auto blue2 = get_color(Colors::BLUE, 1.5);
            auto red2 = get_color(Colors::RED, 1.5);
            THEN("the colors are not scaled above 100%") {
                REQUIRE(white == white2);
                REQUIRE(blue == blue2);
                REQUIRE(red == red2);
            }
        }
    }
    GIVEN("some colors and half brightness") {
        auto white = get_color(Colors::SOFT_WHITE, 0.5);
        auto blue = get_color(Colors::BLUE, 0.5);
        auto red = get_color(Colors::RED, 0.5);
        THEN("the result is as expected") {
            REQUIRE(white == xt1511::XT1511{.w = 0xEE / 2});
            REQUIRE(blue == xt1511::XT1511{.b = 0xFF / 2});
            REQUIRE(red == xt1511::XT1511{.r = 0x50 / 2});
        }
    }
    GIVEN("some colors and zero brightness") {
        auto white = get_color(Colors::SOFT_WHITE, 0);
        auto blue = get_color(Colors::BLUE, 0);
        auto red = get_color(Colors::RED, 0);
        THEN("the result is as expected") {
            REQUIRE(white == xt1511::XT1511{});
            REQUIRE(blue == xt1511::XT1511{});
            REQUIRE(red == xt1511::XT1511{});
        }
    }
}
