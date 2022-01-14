#include <cstdbool>
#include <iostream>

#include "catch2/catch.hpp"
#include "core/xt1511.hpp"
#include "test/test_xt1511_policy.hpp"

using namespace xt1511;

SCENARIO("XT1511 structure works") {
    GIVEN("a default XT1511 structure") {
        XT1511 led{};
        THEN("all of the color values are 0") {
            REQUIRE(led.w == 0);
            REQUIRE(led.r == 0);
            REQUIRE(led.g == 0);
            REQUIRE(led.b == 0);
        }
        WHEN("setting values to 10") {
            led.r = led.b = led.g = led.w = 10;
            AND_WHEN("setting the scale to 2x") {
                led.set_scale(2);
                THEN("the values are doubled to 20") {
                    REQUIRE(led.w == 20);
                    REQUIRE(led.r == 20);
                    REQUIRE(led.g == 20);
                    REQUIRE(led.b == 20);
                }
            }
        }
    }
}

SCENARIO("XT1511String driver works") {
    GIVEN("an XT1511String with 1 pixels, half-speed, 16-bit PWM values") {
        constexpr size_t led_count = 1;
        auto leds = XT1511String<uint16_t, led_count>(Speed::HALF);
        THEN("the pwm values match the half-speed spec") {
            REQUIRE(leds.pwm_off_percentage() == leds.PWM_OFF_HALF_SPEED);
            REQUIRE(leds.pwm_on_percentage() == leds.PWM_ON_HALF_SPEED);
        }
    }
    GIVEN("an XT1511String with 16 pixels, full-speed, and 16-bit PWM values") {
        constexpr size_t led_count = 16;
        constexpr uint16_t max_pwm = 1000;
        auto leds = XT1511String<uint16_t, led_count>(Speed::FULL);
        uint16_t off_value = leds.pwm_off_percentage() * max_pwm;
        uint16_t on_value = leds.pwm_on_percentage() * max_pwm;
        constexpr size_t pwm_count = 32 * 16;  // Does not include stop bit
        auto policy = TestXT1511Policy<led_count>(max_pwm);
        THEN("the pwm values match the full-speed spec") {
            REQUIRE(leds.pwm_off_percentage() == leds.PWM_OFF_FULL_SPEED);
            REQUIRE(leds.pwm_on_percentage() == leds.PWM_ON_FULL_SPEED);
        }
        WHEN("writing the default pixels") {
            REQUIRE(leds.write(policy));
            THEN("the output is 32*16 0's and then stop bits") {
                for (size_t i = 0; i < pwm_count; ++i) {
                    REQUIRE(policy.buffer().at(i) == off_value);
                }
                REQUIRE(policy.buffer().at(pwm_count) == leds.PWM_STOP_VALUE);
            }
            THEN(
                "the end_send command has been called at end of transmission") {
                REQUIRE(!policy.active());
            }
        }
        WHEN("updating the first pixel") {
            auto pixel = XT1511{.g = 0x1, .r = 0x2, .b = 0x4, .w = 0x8};
            leds.pixel(0) = pixel;
            auto set_indices = std::array{7, 14, 21, 28};
            THEN("writing out the pixels") {
                REQUIRE(leds.write(policy));
                THEN("data is correct") {
                    for (size_t i = 0; i < pwm_count; ++i) {
                        if (std::find(set_indices.begin(), set_indices.end(),
                                      i) != set_indices.end()) {
                            REQUIRE(policy.buffer().at(i) == on_value);
                        } else {
                            REQUIRE(policy.buffer().at(i) == off_value);
                        }
                    }
                    REQUIRE(policy.buffer().at(pwm_count) ==
                            leds.PWM_STOP_VALUE);
                }
            }
            THEN("setting all the pixels to fully on") {
                pixel.g = 0xFF;
                pixel.r = 0xFF;
                pixel.b = 0xFF;
                pixel.w = 0xFF;
                leds.set_all(pixel);
                THEN("writing out the pixels") {
                    REQUIRE(leds.write(policy));
                    THEN("data is correct") {
                        for (size_t i = 0; i < pwm_count; ++i) {
                            REQUIRE(policy.buffer().at(i) == on_value);
                        }
                        REQUIRE(policy.buffer().at(pwm_count) ==
                                leds.PWM_STOP_VALUE);
                    }
                }
            }
        }
    }
}
